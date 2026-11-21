/* sys.c — 官方 API 落地
 *
 * 音量：IMMDeviceEnumerator::GetDefaultAudioEndpoint(eRender,eConsole)
 *       -> IMMDevice::Activate(IID_IAudioEndpointVolume)
 *       -> Get/SetMasterVolumeLevelScalar，归一化 0.0–1.0（endpointvolume.h，Vista+）
 * 亮度：WmiMonitorBrightnessMethods::WmiSetBrightness(uint32 Timeout 秒,
 *       uint8 Brightness 百分比)，命名空间 Root\wmi，返回 0 成功（Vista+）
 *       读初值 WQL 查 WmiMonitorBrightness.CurrentBrightness（uint8）
 *       调用链 CoInitializeEx -> CoCreateInstance(CLSID_WbemLocator)
 *       -> ConnectServer -> GetObject/GetMethod/SpawnInstance/Put -> ExecMethod
 * 兜底：dxva2.dll 动态加载 Get/SetMonitorBrightness；官方文档明确警告
 *       许多显示器未完整实现 VESA MCCS I2C，故只作 WMI 失败后第二路。
 * 进程：ShellExecuteW("open","taskmgr.exe")，成功返回 >32。
 */
#include "sys.h"
#include "config.h"
#include <physicalmonitorenumerationapi.h>   /* PHYSICAL_MONITOR */

typedef BOOL (WINAPI *pGetNum)(HMONITOR, LPDWORD);
typedef BOOL (WINAPI *pGetMons)(HMONITOR, DWORD, LPPHYSICAL_MONITOR);
typedef BOOL (WINAPI *pGetBri)(HANDLE, LPDWORD, LPDWORD, LPDWORD);
typedef BOOL (WINAPI *pSetBri)(HANDLE, DWORD);
typedef BOOL (WINAPI *pDestroy)(DWORD, LPPHYSICAL_MONITOR);

void sys_init(Sys *s)
{
    HRESULT hr;
    ZeroMemory(s, sizeof(*s));
    s->vol = 0.50;
    s->bri = 50.0;

    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    s->comReady = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
    CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT,
                         RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);

    /* Core Audio */
    {
        IMMDeviceEnumerator *pe = NULL;
        IMMDevice *pd = NULL;
        hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                              &IID_IMMDeviceEnumerator, (void **)&pe);
        if (SUCCEEDED(hr) && pe) {
            hr = pe->lpVtbl->GetDefaultAudioEndpoint(pe, eRender, eConsole, &pd);
            if (SUCCEEDED(hr) && pd) {
                hr = pd->lpVtbl->Activate(pd, &IID_IAudioEndpointVolume,
                                          CLSCTX_ALL, NULL, (void **)&s->epVol);
                if (SUCCEEDED(hr) && s->epVol) s->volOK = 1;
                pd->lpVtbl->Release(pd);
            }
            pe->lpVtbl->Release(pe);
        }
        if (s->volOK) {
            float f = 0.5f;
            hr = s->epVol->lpVtbl->GetMasterVolumeLevelScalar(s->epVol, &f);
            if (SUCCEEDED(hr)) s->vol = qs_clamp((double)f, 0.0, 1.0);
        }
    }

    /* WMI 亮度 */
    {
        IWbemLocator *pl = NULL;
        IWbemClassObject *pc = NULL;
        BSTR ns = SysAllocString(L"ROOT\\WMI");
        BSTR cls = SysAllocString(L"WmiMonitorBrightnessMethods");
        BSTR mth = SysAllocString(L"WmiSetBrightness");

        hr = CoCreateInstance(&CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER,
                              &IID_IWbemLocator, (void **)&pl);
        if (SUCCEEDED(hr) && pl) {
            hr = pl->lpVtbl->ConnectServer(pl, ns, NULL, NULL, NULL,
                                          0, NULL, NULL, &s->wmiSvc);
            if (SUCCEEDED(hr) && s->wmiSvc) {
                CoSetProxyBlanket((IUnknown *)s->wmiSvc, RPC_C_AUTHN_WINNT,
                                  RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL,
                                  RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
                hr = s->wmiSvc->lpVtbl->GetObject(s->wmiSvc, cls, 0, NULL, &pc, NULL);
                if (SUCCEEDED(hr) && pc) {
                    hr = pc->lpVtbl->GetMethod(pc, mth, 0, &s->wmiInSig, NULL);
                    if (SUCCEEDED(hr) && s->wmiInSig) s->wmiOK = 1;
                    pc->lpVtbl->Release(pc);
                }
            }
            pl->lpVtbl->Release(pl);
        }
        SysFreeString(ns); SysFreeString(cls); SysFreeString(mth);

        if (s->wmiOK) {
            IEnumWbemClassObject *pe = NULL;
            BSTR wql = SysAllocString(L"WQL");
            BSTR q = SysAllocString(
                L"SELECT CurrentBrightness FROM WmiMonitorBrightness WHERE Active=True");
            /* 0x10 = 官方 WMI 半同步标志常量，与 FORWARD_ONLY 组合 */
            hr = s->wmiSvc->lpVtbl->ExecQuery(s->wmiSvc, wql, q,
                                              WBEM_FLAG_FORWARD_ONLY | 0x10,
                                              NULL, &pe);
            if (SUCCEEDED(hr) && pe) {
                IWbemClassObject *obj = NULL;
                ULONG ret = 0;
                hr = pe->lpVtbl->Next(pe, WBEM_INFINITE, 1, &obj, &ret);
                if (SUCCEEDED(hr) && obj && ret) {
                    VARIANT v;
                    VariantInit(&v);
                    hr = obj->lpVtbl->Get(obj, L"CurrentBrightness", 0, &v, NULL, NULL);
                    if (SUCCEEDED(hr)) {
                        if (v.vt == VT_UI1) s->bri = (double)v.bVal;
                        else if (v.vt == VT_I4) s->bri = (double)v.lVal;
                        else if (v.vt == VT_UI4) s->bri = (double)v.ulVal;
                        s->bri = qs_clamp(s->bri, 0.0, 100.0);
                    }
                    VariantClear(&v);
                    obj->lpVtbl->Release(obj);
                }
                pe->lpVtbl->Release(pe);
            }
            SysFreeString(wql); SysFreeString(q);
        }
    }

    /* DDC/CI 兜底：动态加载，不产生硬依赖 */
    s->dxva2 = LoadLibraryW(L"dxva2.dll");
    if (s->dxva2) {
        s->fnGetNum   = (void *)GetProcAddress(s->dxva2, "GetNumberOfPhysicalMonitorsFromHMONITOR");
        s->fnGetMons  = (void *)GetProcAddress(s->dxva2, "GetPhysicalMonitorsFromHMONITOR");
        s->fnGetBri   = (void *)GetProcAddress(s->dxva2, "GetMonitorBrightness");
        s->fnSetBri   = (void *)GetProcAddress(s->dxva2, "SetMonitorBrightness");
        s->fnDestroy  = (void *)GetProcAddress(s->dxva2, "DestroyPhysicalMonitors");
        s->ddcOK = s->fnGetNum && s->fnGetMons && s->fnGetBri &&
                   s->fnSetBri && s->fnDestroy;
    }
}

void sys_shutdown(Sys *s)
{
    if (s->wmiInSig) { s->wmiInSig->lpVtbl->Release(s->wmiInSig); s->wmiInSig = NULL; }
    if (s->wmiSvc)   { s->wmiSvc->lpVtbl->Release(s->wmiSvc);     s->wmiSvc = NULL; }
    if (s->epVol)    { s->epVol->lpVtbl->Release(s->epVol);       s->epVol = NULL; }
    if (s->dxva2)    { FreeLibrary(s->dxva2); s->dxva2 = NULL; }
    if (s->comReady) CoUninitialize();
}

int sys_set_volume(Sys *s, double f)
{
    if (!s->volOK || !s->epVol) return 0;
    return SUCCEEDED(s->epVol->lpVtbl->SetMasterVolumeLevelScalar(
        s->epVol, (float)qs_clamp(f, 0.0, 1.0), NULL));
}

int sys_set_brightness(Sys *s, int pct, HWND hwnd)
{
    IWbemClassObject *pin = NULL, *pout = NULL;
    BSTR cls = NULL, mth = NULL;
    VARIANT v;
    HRESULT hr;
    int ok = 0;

    if (s->wmiOK && s->wmiSvc && s->wmiInSig) {
        cls = SysAllocString(L"WmiMonitorBrightnessMethods");
        mth = SysAllocString(L"WmiSetBrightness");
        hr = s->wmiInSig->lpVtbl->SpawnInstance(s->wmiInSig, 0, &pin);
        if (SUCCEEDED(hr) && pin) {
            VariantInit(&v);
            v.vt = VT_UI1; v.bVal = (BYTE)qs_iclamp(pct, 0, 100);
            pin->lpVtbl->Put(pin, L"Brightness", 0, &v, 0);
            VariantInit(&v);
            v.vt = VT_UI4; v.ulVal = 1;       /* Timeout = 1 秒 */
            pin->lpVtbl->Put(pin, L"Timeout", 0, &v, 0);
            hr = s->wmiSvc->lpVtbl->ExecMethod(s->wmiSvc, cls, mth, 0, NULL,
                                               pin, &pout, NULL);
            if (SUCCEEDED(hr)) {
                ok = 1;
                if (pout) {
                    VARIANT rv;
                    VariantInit(&rv);
                    hr = pout->lpVtbl->Get(pout, L"ReturnValue", 0, &rv, NULL, NULL);
                    if (SUCCEEDED(hr) && (rv.vt == VT_I4 || rv.vt == VT_UI4)) {
                        LONG rc = (rv.vt == VT_I4) ? rv.lVal : (LONG)rv.ulVal;
                        ok = (rc == 0);
                    }
                    VariantClear(&rv);
                    pout->lpVtbl->Release(pout);
                }
            }
            pin->lpVtbl->Release(pin);
        }
        SysFreeString(cls); SysFreeString(mth);
        if (ok) return 1;
        s->wmiOK = 0;        /* WMI 失败后转 DDC/CI */
    }

    if (s->ddcOK && hwnd) {
        HMONITOR hm = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
        DWORD n = 0;
        if (((pGetNum)s->fnGetNum)(hm, &n) && n > 0) {
            LPPHYSICAL_MONITOR arr =
                (LPPHYSICAL_MONITOR)calloc(n, sizeof(PHYSICAL_MONITOR));
            if (arr) {
                if (((pGetMons)s->fnGetMons)(hm, n, arr)) {
                    for (DWORD i = 0; i < n; i++) {
                        DWORD mn = 0, cur = 0, mx = 100;
                        if (((pGetBri)s->fnGetBri)(arr[i].hPhysicalMonitor,
                                                    &mn, &cur, &mx) && mx > mn) {
                            DWORD val = mn + (DWORD)((mx - mn) * pct / 100.0 + 0.5);
                            if (((pSetBri)s->fnSetBri)(arr[i].hPhysicalMonitor, val))
                                ok = 1;
                        }
                    }
                    ((pDestroy)s->fnDestroy)(n, arr);
                }
                free(arr);
            }
        }
    }
    return ok;
}

void sys_apply_volume(Sys *s, int force)
{
    DWORD n = qs_now();
    if (!force && n - s->lastApply < APPLY_THROTTLE) return;
    s->lastApply = n;
    sys_set_volume(s, s->vol);
}
void sys_apply_brightness(Sys *s, int force, HWND hwnd)
{
    DWORD n = qs_now();
    if (!force && n - s->lastApply < APPLY_THROTTLE) return;
    s->lastApply = n;
    sys_set_brightness(s, (int)(s->bri + 0.5), hwnd);
}
void sys_open_taskmgr(void)
{
    ShellExecuteW(NULL, L"open", L"taskmgr.exe", NULL, NULL, SW_SHOWNORMAL);
}
void sys_open_repo(void)
{
    /* URL 走 "open" 动词，由 shell 关联的默认浏览器打开；成功返回 >32。
       地址取自 config.h 的 QISLAND_REPO_URL，与 about.c 显示的文字同源。 */
    ShellExecuteW(NULL, L"open", QISLAND_REPO_URL, NULL, NULL, SW_SHOWNORMAL);
}
