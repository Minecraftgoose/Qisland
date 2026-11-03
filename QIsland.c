/* =========================================================================
 *  QIsland — Windows 灵动岛（音乐版）
 *  纯 Win32 + GDI，无第三方运行时依赖，参数硬编码。
 *
 *  静默（无音乐）  → 岛显示时钟
 *  播放音乐        → 岛显示封面 + 歌名（过长来回滚动）
 *  双击展开        → 音乐卡片：大封面 / 歌名 / 歌手 / 三键 / 进度条
 *
 *  数据来源：Windows SMTC（系统媒体传输控制）。所有向系统注册了媒体会话的
 *  播放器都能读到（汽水音乐、网易云、QQ音乐、Spotify、浏览器…）。
 *  通过常驻 PowerShell 采集进程读取（微软官方 WinRT 投影，接口绝对可靠），
 *  主程序读其输出的 JSON 与封面文件。采集进程挂掉会自动重启。
 *
 *  命令行：
 *    --clock / --date / --text "文字"   静默态内容
 *    --hide                            启动即隐藏
 *    --demo                            用模拟歌曲验证 UI（不依赖真实播放器）
 *    --diag                            输出一次 SMTC 诊断后退出
 *    --dump <file.bmp>                 渲染当前状态到 BMP 后退出
 *
 *  编译：x86_64-w64-mingw32-gcc -O2 -s -mwindows -municode island.c \
 *        -o QIsland.exe -luser32 -lgdi32 -lshell32 -lole32 -lgdiplus -lshlwapi -lm
 * ========================================================================= */
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <wchar.h>
#include <stdarg.h>
#include <tlhelp32.h>
#include <shlobj.h>

#define APP_NAME    L"QIsland"
#define WND_CLASS   L"WinIslandLayeredClass"
#define WM_WAKE     (WM_APP + 1)
#define WM_COLLAPSE (WM_APP + 4)
#define TIMER_TICK  1
#define HOTKEY_ID   1

#define IDM_EXPAND  1001
#define IDM_TOGGLE  1002
#define IDM_EXIT    1003

/* --------------------------------------------------- 外观（全部硬编码） */
#define ISLAND_W        200
#define ISLAND_H        32
#define ISLAND_R        16

#define CARD_W          420
#define CARD_H          260
#define CARD_R          22

#define ISLAND_INSET    10
#define ISLAND_COLOR    RGB(0, 0, 0)
#define ISLAND_OPACITY  255

#define ISLAND_FONT     L"Segoe UI"
#define ISLAND_FSIZE    13
#define CARD_TITLE_FS   19
#define CARD_ARTIST_FS  13
#define ISLAND_FWEIGHT  600
#define TEXT_COLOR      RGB(255, 255, 255)
#define SUB_TEXT_COLOR  RGB(150, 150, 158)
#define BTN_COLOR       RGB(38, 38, 42)
#define BTN_HOVER       RGB(58, 58, 64)
#define BAR_COLOR       RGB(58, 58, 64)
#define BAR_FILL        RGB(255, 255, 255)

/* 窗口透明内边距：弹起 / 展开过冲时会超出内容尺寸，必须留余量否则被裁 */
#define PADX            52
#define PADY            32

#define SS              8       /* 边缘超采样倍率 */
#define SOFT_PX         1.6     /* 边缘过渡带宽度（输出像素） */
#define BAND_PAD        1.0

/* ------------------------------------------------ 弹簧（点击反馈） */
#define PRESS_POP       0.075
#define PRESS_HOLD      0.028
#define POP_HOLD_MS     200.0
#define POP_GROW_STIFF  110.0
#define REST_STIFF      150.0
#define SPRING_DAMP     0.42
#define JELLY_K         0.13
#define JELLY_MAX       0.09

/* ------------------------------------------------ 弹簧（展开卡片） */
#define EXP_STIFF       120.0
#define EXP_DAMP        0.62
#define CLOCK_FADE_T    0.25
#define SUBSTEP_HZ      240.0

/* ------------------------------------------------ 媒体相关 */
#define POLL_MS         700     /* 采集间隔 */
#define SCROLL_SPEED    26.0    /* 歌名滚动速度 px/s */
#define SCROLL_PAUSE    1400.0  /* 两端停顿 ms */
#define TITLE_MAX       160
#define TEXT_MAX        160

/* ------------------------------------------------------------ 全局状态 */
typedef enum { TM_TEXT = 0, TM_TIME, TM_DATE } TextMode;

static HWND   g_hwnd = NULL;
static int    g_visible = 1;
static int    g_mode = TM_TIME;
static WCHAR  g_text[TEXT_MAX];
static int    g_demo = 0;
static int    g_noCover = 0;
static int    g_probe = 0;
static int    SMTC_Available(void);   /* 前向声明：启动采集进程前要判断 */

static double g_scale = 1.0;
static WCHAR  g_lastTxt[TEXT_MAX];

static double press = 0, pressV = 0, sq = 0;
static double popUntil = -1e9;
static int    holding = 0;
static int    g_expanded = 0;
static double expP = 0, expV = 0;

static double g_lastT = 0;
static int    g_anim = 0;

static int    g_px = 0, g_py = 0, g_pw = 0, g_ph = 0;
static double g_cx = 0, g_cy = 0, g_RX = 0, g_RY = 0, g_r = 0;

static HHOOK  g_hook = NULL;

/* ------------------------------------------------------------ 媒体状态 */
typedef struct {
    int    has;          /* 有有效媒体 */
    int    playing;
    WCHAR  title[TITLE_MAX];
    WCHAR  artist[TITLE_MAX];
    double pos;          /* 秒 */
    double dur;          /* 秒 */
    double stamp;        /* 上次更新时刻 ms */
    int    coverW, coverH;
    unsigned char *cover;   /* BGRA 非预乘，top-down */
    unsigned int  coverId;  /* 封面变更计数 */
    int    src;             /* 0=无 1=SMTC 2=窗口标题 3=本地队列 */
    WCHAR  coverUrl[512];
} Media;

static Media       g_media;
static CRITICAL_SECTION g_mediaCS;

/* 命令：写给采集进程执行（1=上一首 2=播放暂停 3=下一首，4+pos=跳转） */
static int    g_cmdPending = 0;
static double g_cmdPos = 0;

/* ---------------------------------------------------- GDI+（动态加载） */
/* 只在需要解码封面时才加载，避免 wine / 精简环境下因缺 gdiplus 直接起不来 */
typedef struct {
    UINT32 GdiplusVersion;
    void  *DebugEventCallback;
    BOOL   SuppressBackgroundThread;
    BOOL   SuppressExternalCodecs;
} GpStartupInput;
typedef struct { void *NotificationHook; void *NotificationUnhook; } GpStartupOutput;

typedef int  GpStatus;
typedef void GpBitmap;

static HMODULE g_gpMod = NULL;
static GpStatus (WINAPI *fGpStartup)(ULONG_PTR *, const GpStartupInput *, GpStartupOutput *);
static void     (WINAPI *fGpShutdown)(ULONG_PTR);
static GpStatus (WINAPI *fGpCreateFromFile)(const WCHAR *, GpBitmap **);
static GpStatus (WINAPI *fGpCreateHBmp)(GpBitmap *, HBITMAP *, DWORD);
static GpStatus (WINAPI *fGpDispose)(GpBitmap *);
static ULONG_PTR g_gpTok = 0;
static int       g_gpOK = 0;

static int GdiplusInit(void)
{
    if (g_gpOK) return 1;
    if (g_gpMod) return 0;                 /* 已尝试过且失败 */
    g_gpMod = LoadLibraryW(L"gdiplus.dll");
    if (!g_gpMod) return 0;
    fGpStartup        = (void *)GetProcAddress(g_gpMod, "GdiplusStartup");
    fGpShutdown       = (void *)GetProcAddress(g_gpMod, "GdiplusShutdown");
    fGpCreateFromFile = (void *)GetProcAddress(g_gpMod, "GdipCreateBitmapFromFile");
    fGpCreateHBmp     = (void *)GetProcAddress(g_gpMod, "GdipCreateHBITMAPFromBitmap");
    fGpDispose        = (void *)GetProcAddress(g_gpMod, "GdipDisposeImage");
    if (!fGpStartup || !fGpCreateFromFile || !fGpCreateHBmp || !fGpDispose) {
        FreeLibrary(g_gpMod); g_gpMod = NULL; return 0;
    }
    GpStartupInput si;
    si.GdiplusVersion = 1;
    si.DebugEventCallback = NULL;
    si.SuppressBackgroundThread = FALSE;
    si.SuppressExternalCodecs = FALSE;
    if (fGpStartup(&g_gpTok, &si, NULL) != 0) {
        FreeLibrary(g_gpMod); g_gpMod = NULL; return 0;
    }
    g_gpOK = 1;
    return 1;
}

/* ------------------------------------------------------------ 工具 */
static double clampd(double v, double a, double b) { return v < a ? a : (v > b ? b : v); }
static double lerpd(double a, double b, double t) { return a + (b - a) * t; }
static double now_ms(void) { return (double)GetTickCount64(); }

/* ------------------------------------------------------- DPI */
typedef BOOL    (WINAPI *fnSetCtx)(HANDLE);
typedef HRESULT (WINAPI *fnGetDpi)(HMONITOR, int, UINT *, UINT *);
typedef BOOL    (WINAPI *fnSetAware)(void);
static fnGetDpi pGetDpi = NULL;

static void InitDpiAware(void)
{
    HMODULE h = LoadLibraryW(L"user32.dll");
    if (h) {
        fnSetCtx p = (fnSetCtx)(void *)GetProcAddress(h, "SetProcessDpiAwarenessContext");
        if (!p || !p((HANDLE)(LONG_PTR)-4)) {
            fnSetAware q = (fnSetAware)(void *)GetProcAddress(h, "SetProcessDPIAware");
            if (q) q();
        }
    }
    HMODULE hs = LoadLibraryW(L"shcore.dll");
    if (hs) pGetDpi = (fnGetDpi)(void *)GetProcAddress(hs, "GetDpiForMonitor");
}

static void GetScreen(RECT *rc, double *scale)
{
    MONITORINFOEXW mi;
    ZeroMemory(&mi, sizeof(mi));
    mi.cbSize = sizeof(mi);
    HMONITOR mon = MonitorFromPoint((POINT){0, 0}, MONITOR_DEFAULTTOPRIMARY);
    if (!GetMonitorInfoW(mon, (LPMONITORINFO)&mi))
        SetRect(rc, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    else
        *rc = mi.rcMonitor;

    UINT dx = 96, dy = 96;
    if (pGetDpi) {
        if (pGetDpi(mon, 0, &dx, &dy) != S_OK || dx == 0) dx = 96;
    } else {
        HDC hdc = GetDC(NULL);
        if (hdc) { dx = (UINT)GetDeviceCaps(hdc, LOGPIXELSX); ReleaseDC(NULL, hdc); }
    }
    *scale = (dx < 96 ? 96 : dx) / 96.0;
}

/* ============================================================
 *  PowerShell 采集脚本
 * ========================================================== */
static const char *PS_SCRIPT =
"$ErrorActionPreference='SilentlyContinue'\n"
"param([string]$OutPath,[string]$CoverPath,[string]$CmdPath,[int]$IntervalMs=700)\n"
"Add-Type -AssemblyName System.Runtime.WindowsRuntime | Out-Null\n"
"$null=[Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager,Windows.Media.Control,ContentType=WindowsRuntime]\n"
"$null=[Windows.Storage.Streams.DataReader,Windows.Storage.Streams,ContentType=WindowsRuntime]\n"
"$null=[Windows.Storage.Streams.Buffer,Windows.Storage.Streams,ContentType=WindowsRuntime]\n"
"$m=[System.WindowsRuntimeSystemExtensions].GetMethods()|Where-Object{$_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncOperation`1'}|Select-Object -First 1\n"
"function Await($op,$ty){ if(-not $op){return $null}; $g=$m.MakeGenericMethod($ty); $t=$g.Invoke($null,@($op)); $t.Wait(-1)|Out-Null; return $t.Result }\n"
"$T_SM=[Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager,Windows.Media.Control,ContentType=WindowsRuntime]\n"
"$T_MP=[Windows.Media.Control.GlobalSystemMediaTransportControlsSessionMediaProperties,Windows.Media.Control,ContentType=WindowsRuntime]\n"
"$T_ST=[Windows.Storage.Streams.IRandomAccessStreamWithContent,Windows.Storage.Streams,ContentType=WindowsRuntime]\n"
"$T_UI=[System.UInt32]\n"
"$utf8=New-Object System.Text.UTF8Encoding($false)\n"
"function Esc($s){ if($null -eq $s){return ''}; $s=[string]$s; $s=$s -replace '\\\\','\\\\\\\\'; $s=$s -replace '\"','\\\\\"'; return $s }\n"
"$lastCover=''\n"
"while($true){\n"
"  try{\n"
"    if(Test-Path $CmdPath){\n"
"      $c=(Get-Content $CmdPath -Raw | ConvertFrom-Json)\n"
"      Remove-Item $CmdPath -Force\n"
"      $mgr=Await $T_SM::RequestAsync() $T_SM\n"
"      if($mgr){ $s=$mgr.GetCurrentSession()\n"
"        if($s){ if($c.a -eq 1){$null=$s.TrySkipPreviousAsync()}\n"
"               elseif($c.a -eq 2){$null=$s.TryTogglePlayPauseAsync()}\n"
"               elseif($c.a -eq 3){$null=$s.TrySkipNextAsync()}\n"
"               elseif($c.a -eq 4){$null=$s.TryChangePlaybackPositionAsync([int64]$c.p)} } }\n"
"    }\n"
"  }catch{}\n"
"  $has=0;$play=0;$ti='';$ar='';$pos=0;$dur=0\n"
"  try{\n"
"    $mgr=Await $T_SM::RequestAsync() $T_SM\n"
"    if($mgr){\n"
"      $s=$mgr.GetCurrentSession()\n"
"      if($null -eq $s){ $ss=$mgr.GetSessions(); if($ss.Count -gt 0){$s=$ss[0]} }\n"
"      if($s){\n"
"        $pr=Await $s.TryGetMediaPropertiesAsync() $T_MP\n"
"        if($pr){ $ti=[string]$pr.Title; if([string]$pr.Artist){$ar=[string]$pr.Artist}else{$ar=[string]$pr.AlbumArtist} }\n"
"        $pb=$s.GetPlaybackInfo()\n"
"        if($pb -and $pb.PlaybackStatus -eq 4){$play=1}\n"
"        $tl=$s.GetTimelineProperties()\n"
"        if($tl){$pos=$tl.Position.TotalMilliseconds;$dur=$tl.EndTime.TotalMilliseconds}\n"
"        if($ti -or $ar){$has=1}\n"
"        if($pr){\n"
"          $th=$pr.Thumbnail\n"
"          if($th){\n"
"            $stm=Await $th.OpenReadAsync() $T_ST\n"
"            if($stm){\n"
"              $n=[uint32]$stm.Size\n"
"              if($n -gt 0 -and $n -lt 20000000){\n"
"                $rd=New-Object Windows.Storage.Streams.DataReader($stm.GetInputStreamAt(0))\n"
"                $ld=Await $rd.LoadAsync($n) $T_UI\n"
"                $b=New-Object byte[] $n\n"
"                $rd.ReadBytes($b)\n"
"                $key=[System.BitConverter]::ToString([System.Security.Cryptography.MD5]::Create().ComputeHash($b))\n"
"                if($key -ne $lastCover){\n"
"                  $lastCover=$key\n"
"                  [System.IO.File]::WriteAllBytes($CoverPath,$b)\n"
"                  [System.IO.File]::WriteAllText($CoverPath+'.id',[string](Get-Date).Ticks,$utf8)\n"
"                }\n"
"              }\n"
"            }\n"
"          }\n"
"        }\n"
"      }\n"
"    }\n"
"  }catch{}\n"
"  $j='{\"has\":'+$has+',\"playing\":'+$play+',\"pos\":'+[int64]$pos+',\"dur\":'+[int64]$dur+',\"title\":\"'+(Esc $ti)+'\",\"artist\":\"'+(Esc $ar)+'\"}'\n"
"  [System.IO.File]::WriteAllText($OutPath,$j,$utf8)\n"
"  Start-Sleep -Milliseconds $IntervalMs\n"
"}\n";

/* 内嵌脚本写到临时目录 */
static void ScriptPaths(WCHAR *ps1, WCHAR *json, WCHAR *cover, WCHAR *cmd)
{
    WCHAR tmp[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    wcscpy(ps1, tmp);   wcscat(ps1, L"WinIsland_media.ps1");
    wcscpy(json, tmp);  wcscat(json, L"WinIsland_media.json");
    wcscpy(cover, tmp); wcscat(cover, L"WinIsland_cover.img");
    wcscpy(cmd, tmp);   wcscat(cmd, L"WinIsland_cmd.json");
}

static void WriteScript(const WCHAR *path)
{
    HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return;
    DWORD w = 0;
    WriteFile(f, PS_SCRIPT, (DWORD)strlen(PS_SCRIPT), &w, NULL);
    CloseHandle(f);
}

static WCHAR *FindPowerShell(void)
{
    static WCHAR path[MAX_PATH];
    WCHAR sys[MAX_PATH];
    GetSystemDirectoryW(sys, MAX_PATH);
    wcscpy(path, sys);
    wcscat(path, L"\\WindowsPowerShell\\v1.0\\powershell.exe");
    if (PathFileExistsW(path)) return path;
    if (PathFileExistsW(L"powershell.exe")) { wcscpy(path, L"powershell.exe"); return path; }
    if (PathFileExistsW(L"pwsh.exe"))       { wcscpy(path, L"pwsh.exe");       return path; }
    return NULL;
}

/* ---------------------------------------------------- 采集进程管理 */
static PROCESS_INFORMATION g_pi;
static int    g_piAlive = 0;
static WCHAR  g_pJson[MAX_PATH], g_pCover[MAX_PATH], g_pCmd[MAX_PATH];
static double g_lastProcStart = 0;

static void StartCollector(void)
{
    if (!SMTC_Available()) return;      /* 旧系统：别启动注定失败的采集进程 */
    WCHAR ps1[MAX_PATH];
    ScriptPaths(ps1, g_pJson, g_pCover, g_pCmd);
    WriteScript(ps1);

    WCHAR *ps = FindPowerShell();
    if (!ps) return;

    WCHAR args[1600];
    _snwprintf(args, 1600,
        L"-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \"%s\" "
        L"-OutPath \"%s\" -CoverPath \"%s\" -CmdPath \"%s\" -IntervalMs %d",
        ps1, g_pJson, g_pCover, g_pCmd, POLL_MS);

    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    ZeroMemory(&g_pi, sizeof(g_pi));

    if (CreateProcessW(ps, args, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                       NULL, NULL, &si, &g_pi)) {
        g_piAlive = 1;
        g_lastProcStart = now_ms();
    }
}

static void StopCollector(void)
{
    if (g_piAlive) {
        TerminateProcess(g_pi.hProcess, 0);
        CloseHandle(g_pi.hProcess);
        CloseHandle(g_pi.hThread);
        g_piAlive = 0;
    }
}

static void EnsureCollector(void)
{
    if (g_demo) return;
    if (!g_piAlive) {
        if (now_ms() - g_lastProcStart > 3000) StartCollector();
        return;
    }
    DWORD code = 0;
    if (GetExitCodeProcess(g_pi.hProcess, &code) && code != STILL_ACTIVE) {
        CloseHandle(g_pi.hProcess);
        CloseHandle(g_pi.hThread);
        g_piAlive = 0;
        StartCollector();
    }
}

/* --------------------------------------------------------- JSON 解析 */
static const char *jfind(const char *j, const char *key)
{
    char k[64];
    _snprintf(k, 64, "\"%s\"", key);
    const char *p = strstr(j, k);
    if (!p) return NULL;
    p += strlen(k);
    while (*p == ' ' || *p == ':') p++;
    return p;
}

static double jnum(const char *j, const char *key)
{
    const char *p = jfind(j, key);
    if (!p) return 0;
    return strtod(p, NULL);
}

/* JSON 字符串（UTF-8）→ WCHAR */
static void jstr(const char *j, const char *key, WCHAR *out, int cch)
{
    out[0] = 0;
    const char *p = jfind(j, key);
    if (!p || *p != '"') return;
    p++;
    const char *q = p;
    while (*q && *q != '"') { if (*q == '\\') q++; q++; }
    int n = (int)(q - p);
    if (n <= 0) return;
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) return;
    /* 处理转义 */
    int o = 0;
    for (const char *s = p; s < q; ) {
        if (*s == '\\' && s + 1 < q) {
            s++;
            if      (*s == 'n') buf[o++] = '\n';
            else if (*s == 't') buf[o++] = '\t';
            else if (*s == 'r') buf[o++] = '\r';
            else                buf[o++] = *s;
            s++;
        } else buf[o++] = *s++;
    }
    buf[o] = 0;
    MultiByteToWideChar(CP_UTF8, 0, buf, -1, out, cch);
    out[cch - 1] = 0;
    free(buf);
}

/* --------------------------------------------------------- 封面加载 */
static unsigned char *LoadCoverImage(const WCHAR *path, int *w, int *h)
{
    if (!GdiplusInit()) return NULL;

    GpBitmap *bmp = NULL;
    if (fGpCreateFromFile(path, &bmp) != 0 || !bmp) return NULL;

    unsigned char *out = NULL;
    HBITMAP hbm = NULL;
    if (fGpCreateHBmp(bmp, &hbm, 0xFF000000) == 0 && hbm) {
        BITMAP bm;
        ZeroMemory(&bm, sizeof(bm));
        if (GetObjectW(hbm, sizeof(bm), &bm) && bm.bmWidth > 0 && bm.bmHeight > 0) {
            BITMAPINFO bi;
            ZeroMemory(&bi, sizeof(bi));
            bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
            bi.bmiHeader.biWidth       = bm.bmWidth;
            bi.bmiHeader.biHeight      = -bm.bmHeight;
            bi.bmiHeader.biPlanes      = 1;
            bi.bmiHeader.biBitCount    = 32;
            bi.bmiHeader.biCompression = BI_RGB;
            out = (unsigned char *)malloc((size_t)bm.bmWidth * bm.bmHeight * 4);
            if (out) {
                HDC dc = GetDC(NULL);
                if (!GetDIBits(dc, hbm, 0, (UINT)bm.bmHeight, out, &bi, DIB_RGB_COLORS)) {
                    free(out); out = NULL;
                } else {
                    *w = bm.bmWidth; *h = bm.bmHeight;
                }
                if (dc) ReleaseDC(NULL, dc);
            }
        }
        DeleteObject(hbm);
    }
    fGpDispose(bmp);
    return out;
}

/* ============================================================
 *  系统版本检测
 *  SMTC（Windows.Media.Control）要求 Windows 10 1809 / build 17763 及以上。
 *  Win7 / Win8 / 早期 Win10 没有这套 API；Win7 自带 PowerShell 2.0，也没有
 *  System.Runtime.WindowsRuntime，WinRT 投影根本加载不了。
 * ========================================================== */
typedef struct {
    ULONG dwOSVersionInfoSize, dwMajorVersion, dwMinorVersion,
          dwBuildNumber, dwPlatformId;
    WCHAR szCSDVersion[128];
} OSVIW;
typedef LONG (WINAPI *fnRtlGetVersion)(OSVIW *);

static int g_osMajor = 0, g_osMinor = 0, g_osBuild = 0;

static void DetectOS(void)
{
    OSVIW vi; ZeroMemory(&vi, sizeof(vi));
    vi.dwOSVersionInfoSize = sizeof(vi);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    fnRtlGetVersion pRtl = ntdll
        ? (fnRtlGetVersion)(void *)GetProcAddress(ntdll, "RtlGetVersion") : NULL;
    if (pRtl && pRtl(&vi) == 0) {
        g_osMajor = (int)vi.dwMajorVersion;
        g_osMinor = (int)vi.dwMinorVersion;
        g_osBuild = (int)vi.dwBuildNumber;
        return;
    }
    OSVERSIONINFOW ov; ZeroMemory(&ov, sizeof(ov));
    ov.dwOSVersionInfoSize = sizeof(ov);
    if (GetVersionExW(&ov)) {
        g_osMajor = (int)ov.dwMajorVersion;
        g_osMinor = (int)ov.dwMinorVersion;
        g_osBuild = (int)ov.dwBuildNumber;
    }
}

static int SMTC_Available(void)
{
    return (g_osMajor > 10) || (g_osMajor == 10 && g_osBuild >= 17763);
}

/* ============================================================
 *  窗口标题采集（Windows 7 兼容的后备方案）
 *  大多数播放器会把「歌名 - 歌手」放在窗口标题里。
 * ========================================================== */
#ifndef PROCESS_QUERY_LIMITED_INFORMATION
#define PROCESS_QUERY_LIMITED_INFORMATION 0x1000
#endif

typedef struct { WCHAR title[256]; WCHAR exe[64]; int prio; } WinCand;
#define MAXCAND 24
static WinCand g_cands[MAXCAND];
static int    g_ncand = 0;
static CRITICAL_SECTION g_candCS;

/* 优先级：9=浏览器（标签标题，可能不是歌）3=其他播放器 2=国内主流 1=汽水 */
static const struct { const WCHAR *pat; int prio; } PLAYERS[] = {
    { L"sodamusic", 1 }, { L"qishui", 1 },
    { L"cloudmusic", 2 }, { L"qqmusic", 2 }, { L"kugou", 2 }, { L"kwmusic", 2 },
    { L"foobar2000", 3 }, { L"musicbee", 3 }, { L"spotify", 3 }, { L"vlc", 3 },
    { L"aimp", 3 }, { L"potplayer", 3 }, { L"wmplayer", 3 }, { L"itunes", 3 },
    { L"winamp", 3 }, { L"mediamonkey", 3 }, { L"musicfree", 3 },
    { L"yesplaymusic", 3 }, { L"cider", 3 }, { L"lxmusic", 3 },
    { L"chrome", 9 }, { L"msedge", 9 }, { L"firefox", 9 },
    { L"360chrome", 9 }, { L"liebao", 9 }, { L"qqbrowser", 9 }, { L"2345explorer", 9 },
};

static void wlower(WCHAR *s)
{
    for (; *s; s++) if (*s >= 'A' && *s <= 'Z') *s = (WCHAR)(*s + 32);
}
static int icontains(const WCHAR *hay, const WCHAR *needle)
{
    WCHAR a[300], b[64];
    wcsncpy(a, hay, 299); a[299] = 0; wlower(a);
    wcsncpy(b, needle, 63); b[63] = 0; wlower(b);
    return wcsstr(a, b) != NULL;
}

static BOOL CALLBACK EnumWins(HWND h, LPARAM lp)
{
    (void)lp;
    if (g_ncand >= MAXCAND) return FALSE;
    if (h == g_hwnd) return TRUE;
    if (!IsWindowVisible(h)) return TRUE;
    if (GetWindow(h, GW_OWNER)) return TRUE;         /* 只取顶层窗口 */

    WCHAR t[256];
    if (GetWindowTextW(h, t, 256) <= 0) return TRUE;
    if (t[0] == 0) return TRUE;

    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (!pid) return TRUE;

    HANDLE ph = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!ph) ph = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!ph) return TRUE;
    WCHAR path[MAX_PATH];
    DWORD sz = MAX_PATH;
    int ok = QueryFullProcessImageNameW(ph, 0, path, &sz) && sz > 0;
    CloseHandle(ph);
    if (!ok) return TRUE;

    const WCHAR *slash = wcsrchr(path, L'\\');
    const WCHAR *exe = slash ? slash + 1 : path;
    if (exe[0] == 0) return TRUE;
    if (icontains(exe, L"QIsland")) return TRUE;    /* 跳过自己 */

    for (int i = 0; i < (int)(sizeof(PLAYERS)/sizeof(PLAYERS[0])); i++) {
        if (icontains(exe, PLAYERS[i].pat)) {
            EnterCriticalSection(&g_candCS);
            if (g_ncand < MAXCAND) {
                wcsncpy(g_cands[g_ncand].title, t, 255);
                g_cands[g_ncand].title[255] = 0;
                wcsncpy(g_cands[g_ncand].exe, exe, 63);
                g_cands[g_ncand].exe[63] = 0;
                g_cands[g_ncand].prio = PLAYERS[i].prio;
                g_ncand++;
            }
            LeaveCriticalSection(&g_candCS);
            break;
        }
    }
    return TRUE;
}

static void ScanWindows(void)
{
    EnterCriticalSection(&g_candCS);
    g_ncand = 0;
    LeaveCriticalSection(&g_candCS);
    EnumWindows(EnumWins, 0);
}

static const WCHAR *APP_TOKENS[] = {
    L"汽水音乐", L"Soda Music", L"网易云音乐", L"QQ音乐", L"酷狗音乐", L"酷我音乐",
    L"foobar2000", L"PotPlayer", L"Spotify", L"VLC media player", L"AIMP",
    L"MusicBee", L"iTunes", L"Windows Media Player", L"MediaMonkey", L"Winamp",
    L"MusicFree", L"YesPlayMusic", L"Cider", L"LX Music",
    L"Google Chrome", L"Microsoft Edge", L"Mozilla Firefox",
    L"360安全浏览器", L"QQ浏览器", L"2345加速浏览器",
};

static void StripTokens(WCHAR *s)
{
    for (int i = 0; i < (int)(sizeof(APP_TOKENS)/sizeof(APP_TOKENS[0])); i++) {
        const WCHAR *tok = APP_TOKENS[i];
        int tl = (int)wcslen(tok);
        if (!tl) continue;
        for (;;) {
            WCHAR *f = wcsstr(s, tok);
            if (!f) break;
            memmove(f, f + tl, (wcslen(f + tl) + 1) * sizeof(WCHAR));
        }
    }
    for (;;) {
        size_t n = wcslen(s);
        if (!n) break;
        WCHAR c = s[0];
        if (c == L' ' || c == L'-' || c == 0x2013 || c == 0x2014 || c == L'\t') {
            memmove(s, s + 1, n * sizeof(WCHAR));
            continue;
        }
        c = s[n - 1];
        if (c == L' ' || c == L'-' || c == 0x2013 || c == 0x2014 || c == L'\t') {
            s[n - 1] = 0;
            continue;
        }
        break;
    }
}

static int ParseTitle(const WCHAR *raw, WCHAR *title, WCHAR *artist)
{
    WCHAR buf[512];
    wcsncpy(buf, raw, 511); buf[511] = 0;
    StripTokens(buf);
    if (buf[0] == 0) return 0;

    WCHAR *sep = wcsstr(buf, L" - ");
    if (!sep) { WCHAR *s2 = wcsstr(buf, L" \x2013 "); if (s2) sep = s2; }
    if (sep) {
        *sep = 0;
        wcsncpy(title, buf, TITLE_MAX - 1); title[TITLE_MAX - 1] = 0;
        wcsncpy(artist, sep + 3, TITLE_MAX - 1); artist[TITLE_MAX - 1] = 0;
        StripTokens(artist);
    } else {
        wcsncpy(title, buf, TITLE_MAX - 1); title[TITLE_MAX - 1] = 0;
        artist[0] = 0;
    }
    StripTokens(title);
    if (title[0] == 0) return 0;
    if (!_wcsicmp(title, L"GDI+ Window") || !_wcsicmp(title, L"MSCTFIME UI")) return 0;
    return 1;
}

/* 挑最佳候选写入 out，返回是否有 */
static int PickFromWindows(Media *out)
{
    WinCand c[MAXCAND];
    int n;
    EnterCriticalSection(&g_candCS);
    n = g_ncand;
    memcpy(c, g_cands, sizeof(c));
    LeaveCriticalSection(&g_candCS);
    if (n <= 0) return 0;

    int best = 0;
    for (int i = 1; i < n; i++)
        if (c[i].prio < c[best].prio) best = i;
    /* 同优先级取标题最长的 */
    for (int i = 0; i < n; i++)
        if (c[i].prio == c[best].prio &&
            wcslen(c[i].title) > wcslen(c[best].title)) best = i;

    ZeroMemory(out, sizeof(*out));
    if (!ParseTitle(c[best].title, out->title, out->artist)) return 0;
    out->has = 1;
    out->playing = 1;            /* 标题法判断不了暂停，默认按播放处理 */
    out->dur = 0; out->pos = 0;  /* 拿不到进度，卡片会自动隐藏进度条 */
    out->stamp = now_ms();
    return 1;
}

/* ============================================================
 *  汽水音乐本地队列解析 —— LUNA 容器
 *  探测确认：LunaStorage\QueueCache 的文件头是
 *      "LUNA"(4C 55 4E 41) + 标准 gzip 流(1F 8B 08)
 *  所以需要自己实现 gzip/deflate 解压才能看到里面的 JSON。
 *  这套代码不依赖任何系统 API，Win7 起全可用。
 * ========================================================== */

/* ---------------- DEFLATE (inflate) ---------------- */
typedef struct {
    const unsigned char *src; size_t slen, spos;
    unsigned char       *dst; size_t dlen, dpos;
    unsigned int tag; int bitcnt;
    int err;
} INFS;

static int gb(INFS *s, int n)
{
    while (s->bitcnt < n) {
        if (s->spos >= s->slen) { s->err = 1; return 0; }
        s->tag |= (unsigned int)s->src[s->spos++] << s->bitcnt;
        s->bitcnt += 8;
    }
    int v = (int)(s->tag & ((1u << n) - 1u));
    s->tag >>= n;
    s->bitcnt -= n;
    return v;
}

static void pb(INFS *s, unsigned char c)
{
    if (s->dpos >= s->dlen) { s->err = 2; return; }
    s->dst[s->dpos++] = c;
}

typedef struct { unsigned short count[16]; unsigned short symbol[288]; } HUFF;

static int hbuild(HUFF *h, const unsigned char *len, int n)
{
    int offs[16], l;
    for (l = 0; l < 16; l++) h->count[l] = 0;
    for (int i = 0; i < n; i++) h->count[len[i]]++;
    h->count[0] = 0;                       /* 规范：忽略 0 长度 */
    int left = 1;
    for (l = 1; l < 16; l++) {
        left <<= 1;
        left -= h->count[l];
        if (left < 0) return left;
    }
    offs[1] = 0;
    for (l = 1; l < 15; l++) offs[l + 1] = offs[l] + h->count[l];
    for (int i = 0; i < n; i++)
        if (len[i]) h->symbol[offs[len[i]]++] = (unsigned short)i;
    return left;
}

static int hdecode(INFS *s, HUFF *h)
{
    int code = 0, first = 0, index = 0;
    for (int l = 1; l < 16; l++) {
        code |= gb(s, 1);
        int cnt = h->count[l];
        if (code - cnt < first) return h->symbol[index + (code - first)];
        index += cnt; first += cnt; first <<= 1; code <<= 1;
    }
    return -1;
}

int g_infErr = 0;
static const short LB[] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,
                           67,83,99,115,131,163,195,227,258};
static const signed char LE[] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
static const short DB[] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,
                           769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
static const signed char DE[] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
static const unsigned char ORD[] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};

static unsigned char *InfRaw(const unsigned char *src, size_t slen, size_t *outLen)
{
    INFS s;
    ZeroMemory(&s, sizeof(s));
    s.src = src; s.slen = slen;
    s.dlen = 512 * 1024;
    s.dst = (unsigned char *)malloc(s.dlen);
    if (!s.dst) return NULL;

    unsigned char lens[320];
    HUFF hl, hd;

    int last = 0;
    while (!last && !s.err) {
        last = gb(&s, 1);
        int type = gb(&s, 2);
        if (s.err) break;

        if (type == 0) {                                  /* stored */
            s.tag = 0; s.bitcnt = 0;                      /* 字节对齐 */
            unsigned n = (unsigned)s.src[s.spos] | ((unsigned)s.src[s.spos + 1] << 8);
            unsigned nn = (unsigned)s.src[s.spos + 2] | ((unsigned)s.src[s.spos + 3] << 8);
            s.spos += 4;
            if (n != (~nn & 0xFFFFu)) { s.err = 3; break; }
            while (n--) { if (s.spos >= s.slen) { s.err = 1; break; } pb(&s, s.src[s.spos++]); }
        } else if (type == 1 || type == 2) {
            if (type == 1) {
                /* 固定 Huffman：0-143=8, 144-255=9, 256-279=7, 280-287=8 */
                for (int i = 0;   i < 144; i++) lens[i] = 8;
                for (int i = 144; i < 256; i++) lens[i] = 9;
                for (int i = 256; i < 280; i++) lens[i] = 7;
                for (int i = 280; i < 288; i++) lens[i] = 8;
                hbuild(&hl, lens, 288);
                for (int i = 0; i < 30; i++) lens[i] = 5;
                hbuild(&hd, lens, 30);
            } else {
                int nlen = gb(&s, 5) + 257;
                int ndist = gb(&s, 5) + 1;
                int ncode = gb(&s, 4) + 4;
                if (nlen > 286 || ndist > 30) { s.err = 4; break; }
                ZeroMemory(lens, 19);
                for (int i = 0; i < ncode; i++) lens[ORD[i]] = (unsigned char)gb(&s, 3);
                hbuild(&hl, lens, 19);
                int i = 0;
                while (i < nlen + ndist) {
                    int sym = hdecode(&s, &hl);
                    if (sym < 0) { s.err = 5; break; }
                    if (sym < 16) lens[i++] = (unsigned char)sym;
                    else {
                        unsigned char v = 0; int rep = 0;
                        if (sym == 16) { if (!i) { s.err = 6; break; } v = lens[i - 1]; rep = 3 + gb(&s, 2); }
                        else if (sym == 17) rep = 3 + gb(&s, 3);
                        else rep = 11 + gb(&s, 7);
                        if (i + rep > nlen + ndist) { s.err = 7; break; }
                        while (rep--) lens[i++] = v;
                    }
                }
                if (s.err) break;
                if (lens[256] == 0) { s.err = 8; break; }
                hbuild(&hl, lens, nlen);
                hbuild(&hd, lens + nlen, ndist);
            }

            for (;;) {
                int sym = hdecode(&s, &hl);
                if (sym < 0) { s.err = 5; break; }
                if (sym == 256) break;
                if (sym < 256) pb(&s, (unsigned char)sym);
                else {
                    sym -= 257;
                    if (sym >= 29) { s.err = 9; break; }
                    int len = LB[sym] + gb(&s, LE[sym]);
                    int ds = hdecode(&s, &hd);
                    if (ds < 0 || ds >= 30) { s.err = 10; break; }
                    int dist = DB[ds] + gb(&s, DE[ds]);
                    if (dist > (int)s.dpos) { s.err = 11; break; }
                    size_t from = s.dpos - (size_t)dist;
                    while (len--) { pb(&s, s.dst[from++]); if (s.err) break; }
                }
                if (s.err) break;
            }
        } else {
            s.err = 12;                                   /* type 3 保留 */
        }
    }

    g_infErr = s.err;
    if (s.err) { free(s.dst); return NULL; }
    *outLen = s.dpos;
    return s.dst;
}

/* ---------------- LUNA 容器：魔数 + gzip ---------------- */
static unsigned char *LunaRead(const WCHAR *path, size_t *outLen)
{
    *outLen = 0;
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return NULL;
    DWORD sz = GetFileSize(f, NULL);
    if (sz == INVALID_FILE_SIZE || sz < 16 || sz > 8u * 1024 * 1024) { CloseHandle(f); return NULL; }
    unsigned char *raw = (unsigned char *)malloc(sz);
    if (!raw) { CloseHandle(f); return NULL; }
    DWORD rd = 0;
    if (!ReadFile(f, raw, sz, &rd, NULL)) { free(raw); CloseHandle(f); return NULL; }
    CloseHandle(f);

    /* 在开头 32 字节内找 gzip 魔数 */
    size_t p = 0;
    for (p = 0; p + 2 < (size_t)rd && p < 32; p++)
        if (raw[p] == 0x1F && raw[p + 1] == 0x8B && raw[p + 2] == 0x08) break;
    if (p + 2 >= (size_t)rd) { free(raw); return NULL; }

    size_t i = p + 10;                                    /* 跳过 gzip 头 */
    unsigned char flg = raw[p + 3];
    if (flg & 4) {                                        /* FEXTRA */
        if (i + 2 > (size_t)rd) { free(raw); return NULL; }
        i += 2 + (size_t)raw[i] + ((size_t)raw[i + 1] << 8);
    }
    if (flg & 8) { while (i < (size_t)rd && raw[i]) i++; i++; }   /* FNAME */
    if (flg & 16) { while (i < (size_t)rd && raw[i]) i++; i++; }  /* FCOMMENT */
    if (flg & 2) i += 2;                                  /* FHCRC */
    if (i >= (size_t)rd) { free(raw); return NULL; }

    unsigned char *out = InfRaw(raw + i, (size_t)rd - i, outLen);
    free(raw);
    return out;
}

/* ---------------- 极简 JSON 取值 ---------------- */
static const char *ljfind(const char *t, const char *key)
{
    char pat[64];
    _snprintf(pat, 64, "\"%s\"", key);
    return strstr(t, pat);
}
static int ljgetstr(const char *t, const char *key, char *out, int cap)
{
    const char *p = ljfind(t, key);
    if (!p) return 0;
    p += strlen(key) + 2;
    while (*p && *p != ':') p++;
    if (!*p) return 0;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p != '"') return 0;
    p++;
    int o = 0;
    while (*p && *p != '"' && o < cap - 1) {
        if (*p == '\\') {
            p++;
            if (!*p) break;
            char e = *p++;
            if (e == 'n') out[o++] = '\n';
            else if (e == 't') out[o++] = '\t';
            else if (e == 'r') out[o++] = '\r';
            else if (e == 'u') {                          /* \uXXXX → UTF-8 */
                unsigned int cp = 0;
                for (int k = 0; k < 4 && p[k]; k++) {
                    char c = p[k];
                    cp <<= 4;
                    if (c >= '0' && c <= '9') cp |= (unsigned)(c - '0');
                    else if (c >= 'a' && c <= 'f') cp |= (unsigned)(c - 'a' + 10);
                    else if (c >= 'A' && c <= 'F') cp |= (unsigned)(c - 'A' + 10);
                }
                p += 4;
                if (cp < 0x80) out[o++] = (char)cp;
                else if (cp < 0x800) {
                    out[o++] = (char)(0xC0 | (cp >> 6));
                    out[o++] = (char)(0x80 | (cp & 0x3F));
                } else {
                    out[o++] = (char)(0xE0 | (cp >> 12));
                    out[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                    out[o++] = (char)(0x80 | (cp & 0x3F));
                }
            } else out[o++] = e;
        } else out[o++] = *p++;
    }
    out[o] = 0;
    return o > 0;
}
static double ljgetnum(const char *t, const char *key)
{
    const char *p = ljfind(t, key);
    if (!p) return -1;
    p += strlen(key) + 2;
    while (*p && *p != ':') p++;
    if (!*p) return -1;
    p++;
    while (*p == ' ') p++;
    return atof(p);
}

/* ---------------- 进程存活判断 ---------------- */
static int ProcRunning(const WCHAR *pat)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe;
    ZeroMemory(&pe, sizeof(pe));
    pe.dwSize = sizeof(pe);
    int hit = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            WCHAR nm[300], p2[64];
            wcsncpy(nm, pe.szExeFile, 299); nm[299] = 0; wlower(nm);
            wcsncpy(p2, pat, 63); p2[63] = 0; wlower(p2);
            if (wcsstr(nm, p2)) { hit = 1; break; }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return hit;
}

/* ---------------- 从本地队列取当前曲目 ---------------- */
static int PickFromLuna(Media *out)
{
    WCHAR appd[MAX_PATH];
    if (!GetEnvironmentVariableW(L"APPDATA", appd, MAX_PATH)) return 0;
    WCHAR qp[MAX_PATH];
    _snwprintf(qp, MAX_PATH, L"%s\\SodaMusic\\LunaStorage\\QueueCache", appd);
    if (GetFileAttributesW(qp) == INVALID_FILE_ATTRIBUTES) return 0;

    size_t n = 0;
    unsigned char *raw = LunaRead(qp, &n);
    if (!raw || n == 0) return 0;
    char *t = (char *)malloc(n + 1);
    if (!t) { free(raw); return 0; }
    memcpy(t, raw, n);
    t[n] = 0;
    free(raw);

    /* 当前播放的是队列里第几首 */
    double idx = -1;
    const char *ikeys[] = { "playIndex", "currentIndex", "curIndex", "playingIndex", "index" };
    for (int i = 0; i < (int)(sizeof(ikeys)/sizeof(ikeys[0])); i++) {
        idx = ljgetnum(t, ikeys[i]);
        if (idx >= 0) break;
    }

    /* 按 "name" 出现顺序枚举曲目，定位第 idx 首 */
    const char *sel = NULL;
    int cnt = 0;
    const char *p = t;
    while (cnt < 500) {
        const char *nk = ljfind(p, "name");
        if (!nk) nk = ljfind(p, "title");
        if (!nk) break;
        char probe[8];
        int ok = ljgetstr(nk, "name", probe, sizeof(probe));
        if (!ok) ok = ljgetstr(nk, "title", probe, sizeof(probe));
        if (ok) {
            if (idx < 0 || (double)cnt == idx) { sel = nk; break; }
            cnt++;
        }
        p = nk + 6;
    }
    if (!sel && idx < 0) sel = t;

    char name[256] = {0}, artist[256] = {0}, cov[512] = {0};
    double dur = -1;
    if (sel) {
        if (!ljgetstr(sel, "name", name, sizeof(name))) ljgetstr(sel, "title", name, sizeof(name));
        const char *ak[] = { "artist", "singer", "author", "artists" };
        for (int i = 0; i < (int)(sizeof(ak)/sizeof(ak[0])); i++)
            if (ljgetstr(sel, ak[i], artist, sizeof(artist))) break;
        const char *dk[] = { "duration", "durationMs", "duration_ms", "length" };
        for (int i = 0; i < (int)(sizeof(dk)/sizeof(dk[0])); i++) {
            dur = ljgetnum(sel, dk[i]);
            if (dur >= 0) break;
        }
        const char *ck[] = { "coverUrl", "cover", "picUrl", "albumPic", "pic", "imgUrl" };
        for (int i = 0; i < (int)(sizeof(ck)/sizeof(ck[0])); i++)
            if (ljgetstr(sel, ck[i], cov, sizeof(cov))) break;
    }

    int ret = 0;
    if (name[0]) {
        ZeroMemory(out, sizeof(*out));
        int wn = MultiByteToWideChar(CP_UTF8, 0, name, -1, out->title, TITLE_MAX - 1);
        if (wn <= 0) out->title[0] = 0;
        if (artist[0]) {
            int wa = MultiByteToWideChar(CP_UTF8, 0, artist, -1, out->artist, TITLE_MAX - 1);
            if (wa <= 0) out->artist[0] = 0;
        }
        out->dur = (dur > 0) ? dur : 0;
        if (out->dur > 1000) out->dur /= 1000.0;      /* 毫秒 → 秒 */
        out->pos = 0;
        out->has = 1;
        out->playing = ProcRunning(L"sodamusic") ? 1 : 0;
        out->stamp = now_ms();
        if (cov[0]) {
            int wc = MultiByteToWideChar(CP_UTF8, 0, cov, -1, out->coverUrl, 511);
            if (wc <= 0) out->coverUrl[0] = 0;
        }
        ret = 1;
    }
    free(t);
    return ret;
}

/* ---------------- 封面下载（动态加载 wininet，缺了也不影响主功能） ---------------- */
typedef void *HINET;
typedef HINET (WINAPI *fIO)(const char *, DWORD, const char *, const char *, DWORD);
typedef HINET (WINAPI *fIOU)(HINET, const char *, const char *, DWORD, DWORD, DWORD);
typedef BOOL  (WINAPI *fIRF)(HINET, void *, DWORD, DWORD *);
typedef BOOL  (WINAPI *fIC)(HINET);

static int DownloadCover(const char *url, const WCHAR *outPath)
{
    HMODULE m = LoadLibraryW(L"wininet.dll");
    if (!m) return 0;
    fIO  pIO  = (fIO)(void *)GetProcAddress(m, "InternetOpenA");
    fIOU pIOU = (fIOU)(void *)GetProcAddress(m, "InternetOpenUrlA");
    fIRF pIRF = (fIRF)(void *)GetProcAddress(m, "InternetReadFile");
    fIC  pIC  = (fIC)(void *)GetProcAddress(m, "InternetCloseHandle");
    if (!pIO || !pIOU || !pIRF) { FreeLibrary(m); return 0; }

    int ok = 0;
    HINET h = pIO("QIsland", 0, NULL, NULL, 0);
    if (h) {
        HINET u = pIOU(h, url, NULL, 0, 0x80000000 /*INTERNET_FLAG_RELOAD*/, 0);
        if (u) {
            HANDLE f = CreateFileW(outPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                                   FILE_ATTRIBUTE_NORMAL, NULL);
            if (f != INVALID_HANDLE_VALUE) {
                char buf[16384];
                DWORD rd = 0, total = 0;
                while (pIRF(u, buf, sizeof(buf), &rd) && rd > 0 && total < 8u * 1024 * 1024) {
                    DWORD w = 0;
                    WriteFile(f, buf, rd, &w, NULL);
                    total += rd;
                }
                CloseHandle(f);
                ok = (total > 512);
                if (!ok) DeleteFileW(outPath);
            }
            if (pIC) pIC(u);
        }
        if (pIC) pIC(h);
    }
    FreeLibrary(m);
    return ok;
}

/* --------------------------------------------------------- 媒体轮询 */

/* 控制命令：SMTC 可用就走会话，否则发全局媒体键（Win7 也能用） */
static void DoCommand(int a, double pos)
{
    if (SMTC_Available() && g_piAlive) {
        g_cmdPending = a;
        g_cmdPos = pos;
        return;
    }
    if (a == 4) return;                    /* 跳转需要时间轴，标题法做不到 */
    WORD vk = 0;
    if (a == 1)      vk = VK_MEDIA_PREV_TRACK;
    else if (a == 2) vk = VK_MEDIA_PLAY_PAUSE;
    else if (a == 3) vk = VK_MEDIA_NEXT_TRACK;
    if (!vk) return;
    keybd_event((BYTE)vk, 0, 0, 0);
    keybd_event((BYTE)vk, 0, KEYEVENTF_KEYUP, 0);
}

static DWORD WINAPI MediaThread(LPVOID arg)
{
    (void)arg;
    DWORD lastWrite = 0;
    WCHAR lastCovUrl[512] = {0};

    for (;;) {
        if (!g_demo) {
            /* ---- (1) 窗口标题法：Windows 7 起全支持 ---- */
            ScanWindows();
            Media t;
            ZeroMemory(&t, sizeof(t));
            int haveTitle = PickFromWindows(&t);

            /* ---- (2) SMTC：仅 Windows 10 1809+ ---- */
            Media m;
            ZeroMemory(&m, sizeof(m));
            int haveSMTC = 0;

            /* ---- (2) 本地队列：汽水音乐，Win7 也能读 ---- */
            Media lu;
            ZeroMemory(&lu, sizeof(lu));
            int haveLuna = PickFromLuna(&lu);

            if (SMTC_Available()) {
                EnsureCollector();

                HANDLE f = CreateFileW(g_pJson, GENERIC_READ,
                                       FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                if (f != INVALID_HANDLE_VALUE) {
                    DWORD sz = GetFileSize(f, NULL);
                    if (sz > 0 && sz < 65536) {
                        char *buf = (char *)malloc(sz + 1);
                        if (buf) {
                            DWORD rd = 0;
                            if (ReadFile(f, buf, sz, &rd, NULL) && rd > 0) {
                                buf[rd] = 0;
                                m.has     = (int)jnum(buf, "has");
                                m.playing = (int)jnum(buf, "playing");
                                m.pos     = jnum(buf, "pos") / 1000.0;
                                m.dur     = jnum(buf, "dur") / 1000.0;
                                m.stamp   = now_ms();
                                jstr(buf, "title",  m.title,  TITLE_MAX);
                                jstr(buf, "artist", m.artist, TITLE_MAX);
                                if (m.title[0] == 0 && m.artist[0])
                                    wcscpy(m.title, m.artist);
                                haveSMTC = m.has;
                            }
                            free(buf);
                        }
                    }
                    CloseHandle(f);
                }

                /* 封面文件变了才重新解码 */
                WIN32_FILE_ATTRIBUTE_DATA fa;
                if (GetFileAttributesExW(g_pCover, GetFileExInfoStandard, &fa)) {
                    DWORD w = fa.ftLastWriteTime.dwLowDateTime;
                    if (w != lastWrite) {
                        lastWrite = w;
                        int cw = 0, ch = 0;
                        unsigned char *px = LoadCoverImage(g_pCover, &cw, &ch);
                        if (px) {
                            EnterCriticalSection(&g_mediaCS);
                            if (g_media.cover) free(g_media.cover);
                            g_media.cover = px; g_media.coverW = cw; g_media.coverH = ch;
                            g_media.coverId++;
                            LeaveCriticalSection(&g_mediaCS);
                        }
                    }
                }

                /* 待发命令（仅 SMTC 路径需要写文件） */
                if (g_cmdPending) {
                    WCHAR tmp[MAX_PATH + 32];
                    wcscpy(tmp, g_pCmd); wcscat(tmp, L".tmp");
                    HANDLE cf = CreateFileW(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                                            FILE_ATTRIBUTE_NORMAL, NULL);
                    if (cf != INVALID_HANDLE_VALUE) {
                        char body[128];
                        if (g_cmdPending == 4)
                            /* msvcrt 的 _snprintf 不认 %lld，必须用 MS 的 %I64d */
                            _snprintf(body, 128, "{\"a\":4,\"p\":%I64d}",
                                      (__int64)(g_cmdPos * 10000000.0));
                        else
                            _snprintf(body, 128, "{\"a\":%d}", g_cmdPending);
                        DWORD ww = 0;
                        WriteFile(cf, body, (DWORD)strlen(body), &ww, NULL);
                        CloseHandle(cf);
                        MoveFileExW(tmp, g_pCmd, MOVEFILE_REPLACE_EXISTING);
                    }
                    g_cmdPending = 0;
                }
            } else if (g_piAlive) {
                StopCollector();            /* 旧系统：停掉注定失败的采集进程 */
            }

            /* ---- 决策：SMTC > 本地队列 > 窗口标题 ---- */
            Media *use = haveSMTC ? &m : (haveLuna ? &lu : (haveTitle ? &t : NULL));

            EnterCriticalSection(&g_mediaCS);
            unsigned char *keep = g_media.cover;
            int kw = g_media.coverW, kh = g_media.coverH;
            unsigned int kid = g_media.coverId;
            if (use) {
                Media copy = *use;
                g_media = copy;
            } else {
                g_media.has = 0; g_media.playing = 0;
                g_media.title[0] = 0; g_media.artist[0] = 0;
                g_media.pos = 0; g_media.dur = 0; g_media.stamp = now_ms();
            }
            g_media.cover = keep; g_media.coverW = kw; g_media.coverH = kh;
            g_media.coverId = kid;
            g_media.src = haveSMTC ? 1 : (haveLuna ? 3 : (haveTitle ? 2 : 0));
            LeaveCriticalSection(&g_mediaCS);
        }
        /* 本地队列源：按封面 URL 下载（变了吗才下） */
        {
            EnterCriticalSection(&g_mediaCS);
            int src = g_media.src;
            WCHAR u[512];
            wcscpy(u, g_media.coverUrl);
            int hasCov = (g_media.cover && g_media.coverW > 0);
            LeaveCriticalSection(&g_mediaCS);

            if (src == 3 && u[0] && (u[0] != lastCovUrl[0] || wcscmp(u, lastCovUrl) != 0) && !hasCov) {
                WCHAR tmp[MAX_PATH];
                GetTempPathW(MAX_PATH, tmp);
                wcscat(tmp, L"WinIsland_cover.img");
                DeleteFileW(tmp);
                char ua[1024];
                WideCharToMultiByte(CP_UTF8, 0, u, -1, ua, 1024, NULL, NULL);
                if (DownloadCover(ua, tmp)) {
                    int cw = 0, ch = 0;
                    unsigned char *px = LoadCoverImage(tmp, &cw, &ch);
                    if (px) {
                        EnterCriticalSection(&g_mediaCS);
                        if (g_media.cover) free(g_media.cover);
                        g_media.cover = px; g_media.coverW = cw; g_media.coverH = ch;
                        g_media.coverId++;
                        LeaveCriticalSection(&g_mediaCS);
                        wcscpy(lastCovUrl, u);
                    }
                }
            }
        }
        Sleep(300);
    }
    return 0;
}

/* 供 UI 使用的快照 */
static void MediaSnapshot(Media *out)
{
    EnterCriticalSection(&g_mediaCS);
    *out = g_media;
    out->cover = NULL;                 /* 不转移所有权 */
    LeaveCriticalSection(&g_mediaCS);
}

/* 推算当前播放位置（汽水的 SMTC 时间轴刷新不勤，用墙钟补） */
static double MediaPosNow(const Media *m)
{
    if (m->dur <= 0) return 0;
    double p = m->pos;
    if (m->playing) {
        double el = (now_ms() - m->stamp) / 1000.0;
        if (el > 0 && el < 600) p += el;
    }
    if (p < 0) p = 0;
    if (p > m->dur) p = m->dur;
    return p;
}

/* ------------------------------------------------- 圆角矩形 SDF */
static double sdfRoundRect(double px, double py, double x0, double y0,
                           double x1, double y1, double r)
{
    double cx = (x0 + x1) * 0.5, cy = (y0 + y1) * 0.5;
    double hx = (x1 - x0) * 0.5 - r, hy = (y1 - y0) * 0.5 - r;
    if (hx < 0) hx = 0;
    if (hy < 0) hy = 0;
    double dx = fabs(px - cx) - hx, dy = fabs(py - cy) - hy;
    if (dx < 0) dx = 0;
    if (dy < 0) dy = 0;
    return sqrt(dx * dx + dy * dy) - r;
}

static void BlendPM(DWORD *dst, int pw, int ph, int x, int y, int r, int g, int b, double a)
{
    if (x < 0 || y < 0 || x >= pw || y >= ph || a <= 0) return;
    if (a > 1) a = 1;
    DWORD *p = &dst[y * pw + x];
    int pr = (int)((*p >> 16) & 0xFF);
    int pg = (int)((*p >> 8) & 0xFF);
    int pb = (int)(*p & 0xFF);
    int pa = (int)((*p >> 24) & 0xFF);
    double k = 1.0 - a;
    int na = (int)(255.0 * a + pa * k + 0.5);
    int nr = (int)(r * a + pr * k + 0.5);
    int ng = (int)(g * a + pg * k + 0.5);
    int nb = (int)(b * a + pb * k + 0.5);
    if (na > 255) na = 255;
    if (nr > 255) nr = 255;
    if (ng > 255) ng = 255;
    if (nb > 255) nb = 255;
    *p = ((DWORD)na << 24) | ((DWORD)nr << 16) | ((DWORD)ng << 8) | (DWORD)nb;
}

/* ------------------------------------------------------------ 物理 */
static void step(double now, double dt)
{
    double ds = (dt > 40 ? 40 : dt) / 1000.0;
    int steps = (int)ceil(ds * SUBSTEP_HZ);
    if (steps < 1) steps = 1;
    double h = ds / steps;

    int popping = (now < popUntil);
    double pT  = popping ? -PRESS_POP : (holding ? PRESS_HOLD : 0.0);
    double st1 = popping ? POP_GROW_STIFF : REST_STIFF;
    double d1  = 2.0 * sqrt(st1) * SPRING_DAMP;
    for (int i = 0; i < steps; i++) {
        pressV += ((pT - press) * st1 - pressV * d1) * h;
        press  += pressV * h;
    }
    sq = clampd(pressV * JELLY_K, -JELLY_MAX, JELLY_MAX);

    double eT = g_expanded ? 1.0 : 0.0;
    double d2 = 2.0 * sqrt(EXP_STIFF) * EXP_DAMP;
    for (int i = 0; i < steps; i++) {
        expV += ((eT - expP) * EXP_STIFF - expV * d2) * h;
        expP += expV * h;
    }
}

static void shape_geom(double s, double t, double *pRX, double *pRY, double *pr)
{
    double W = lerpd(ISLAND_W, CARD_W, t) * s;
    double H = lerpd(ISLAND_H, CARD_H, t) * s;
    double r = lerpd(ISLAND_H / 2.0, CARD_R, t) * s;
    double HW = W / 2.0 * (1.0 - press);
    double HH = H / 2.0 * (1.0 - press);
    if (r > HW) r = HW;
    if (r > HH) r = HH;
    *pRX = HW * (1.0 + sq * 0.5);
    *pRY = HH * (1.0 - sq * 0.5);
    *pr  = r;
}

static int anim_active(double now)
{
    if (now < popUntil) return 1;
    if (holding)       return 1;
    if (fabs(press) > 1e-4 || fabs(pressV) > 1e-3) return 1;
    if (fabs(expP - (g_expanded ? 1.0 : 0.0)) > 1e-4 || fabs(expV) > 1e-3) return 1;
    return 0;
}

/* --------------------------------------------------------- 文本度量 */
static HFONT MakeFont(int px, int weight)
{
    return CreateFontW(-px, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                       FF_DONTCARE | DEFAULT_PITCH, ISLAND_FONT);
}

/* 文本宽度（设备像素） */
static double TextWidth(HDC dc, const WCHAR *t, int px, int weight)
{
    HFONT f = MakeFont(px, weight);
    HGDIOBJ old = SelectObject(dc, f);
    SIZE sz;
    GetTextExtentPoint32W(dc, t, (int)wcslen(t), &sz);
    SelectObject(dc, old);
    DeleteObject(f);
    return (double)sz.cx;
}

static void BuildClockText(WCHAR *out, size_t cch)
{
    SYSTEMTIME st;
    switch (g_mode) {
    case TM_TIME:
        GetLocalTime(&st);
        _snwprintf(out, cch, L"%02d:%02d", st.wHour, st.wMinute);
        break;
    case TM_DATE: {
        GetLocalTime(&st);
        static const WCHAR *wd[] = {L"日", L"一", L"二", L"三", L"四", L"五", L"六"};
        _snwprintf(out, cch, L"%d月%d日 周%s", st.wMonth, st.wDay, wd[st.wDayOfWeek]);
        break;
    }
    default:
        wcsncpy(out, g_text, cch - 1);
        out[cch - 1] = 0;
        break;
    }
}

/* 在指定矩形内绘制单行文本（居中/左对齐），带裁剪 */
static void DrawTextIn(DWORD *dst, int pw, int ph, const RECT *rc,
                       const WCHAR *t, int px, int weight, COLORREF col,
                       int align, double alpha)
{
    if (!t || !t[0] || alpha <= 0.001) return;
    int w = rc->right - rc->left, h = rc->bottom - rc->top;
    if (w <= 0 || h <= 0) return;

    HDC dc = GetDC(NULL);
    if (!dc) return;

    BITMAPINFO bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    void *tb = NULL;
    HBITMAP dib = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &tb, NULL, 0);
    if (!dib || !tb) { if (dib) DeleteObject(dib); ReleaseDC(NULL, dc); return; }
    memset(tb, 0, (size_t)w * h * 4);

    HDC mem = CreateCompatibleDC(dc);
    HGDIOBJ oldB = SelectObject(mem, dib);
    HFONT f = MakeFont(px, weight);
    SelectObject(mem, f);
    SetBkMode(mem, TRANSPARENT);
    SetTextColor(mem, RGB(255, 255, 255));

    RECT box = {0, 0, w, h};
    UINT fmt = DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_NOCLIP |
               (align == 0 ? DT_CENTER : DT_LEFT);
    DrawTextW(mem, t, -1, &box, fmt);

    int cr = GetRValue(col), cg = GetGValue(col), cb = GetBValue(col);
    for (int y = 0; y < h; y++) {
        int dy = rc->top + y;
        if (dy < 0 || dy >= ph) continue;
        for (int x = 0; x < w; x++) {
            int c = (int)(((DWORD *)tb)[y * w + x] & 0xFF);
            if (c <= 0) continue;
            int dx = rc->left + x;
            if (dx < 0 || dx >= pw) continue;
            BlendPM(dst, pw, ph, dx, dy, cr, cg, cb, (double)c / 255.0 * alpha);
        }
    }
    SelectObject(mem, oldB);
    DeleteObject(f);
    DeleteDC(mem);
    DeleteObject(dib);
    ReleaseDC(NULL, dc);
}

/* --------------------------------------------------------- 封面绘制 */
/* 把封面按 cover-fit 画进 (cx,cy,r) 的圆形区域（抗锯齿边缘） */
static void DrawCoverCircle(DWORD *dst, int pw, int ph,
                            double cx, double cy, double r,
                            const unsigned char *cov, int cw, int ch,
                            double alpha)
{
    double x0 = cx - r, y0 = cy - r, x1 = cx + r, y1 = cy + r;
    int ix0 = (int)floor(x0 - 2), ix1 = (int)ceil(x1 + 2);
    int iy0 = (int)floor(y0 - 2), iy1 = (int)ceil(y1 + 2);
    if (ix0 < 0) ix0 = 0;
    if (iy0 < 0) iy0 = 0;
    if (ix1 >= pw) ix1 = pw - 1;
    if (iy1 >= ph) iy1 = ph - 1;

    double side = 2.0 * r;
    for (int y = iy0; y <= iy1; y++) {
        for (int x = ix0; x <= ix1; x++) {
            double d = sdfRoundRect(x + 0.5, y + 0.5, x0, y0, x1, y1, r);
            double a = clampd(0.5 - d / SOFT_PX, 0.0, 1.0);
            if (a <= 0) continue;

            int r8 = 0, g8 = 0, b8 = 0;
            if (cov && cw > 0 && ch > 0) {
                /* cover-fit：取较长的边为基准 */
                double sc = side / (double)(cw < ch ? cw : ch);
                double sw = cw * sc, sh = ch * sc;
                double u = (x + 0.5 - (cx - sw / 2.0)) / sc;
                double v = (y + 0.5 - (cy - sh / 2.0)) / sc;
                int iu = (int)floor(u), iv = (int)floor(v);
                if (iu < 0) iu = 0;
                if (iu >= cw) iu = cw - 1;
                if (iv < 0) iv = 0;
                if (iv >= ch) iv = ch - 1;
                const unsigned char *p = cov + ((size_t)iv * cw + iu) * 4;
                b8 = p[0]; g8 = p[1]; r8 = p[2];
            }
            BlendPM(dst, pw, ph, x, y, r8, g8, b8, a * alpha);
        }
    }
}

/* 矩形圆角块（按钮 / 进度条底） */
static void FillRoundRect(DWORD *dst, int pw, int ph,
                          double x0, double y0, double x1, double y1, double r,
                          int cr, int cg, int cb, double alpha)
{
    double rr = r;
    double w = x1 - x0, h = y1 - y0;
    if (rr > w / 2.0) rr = w / 2.0;
    if (rr > h / 2.0) rr = h / 2.0;
    int ix0 = (int)floor(x0 - 2), ix1 = (int)ceil(x1 + 2);
    int iy0 = (int)floor(y0 - 2), iy1 = (int)ceil(y1 + 2);
    if (ix0 < 0) ix0 = 0;
    if (iy0 < 0) iy0 = 0;
    if (ix1 >= pw) ix1 = pw - 1;
    if (iy1 >= ph) iy1 = ph - 1;
    for (int y = iy0; y <= iy1; y++)
        for (int x = ix0; x <= ix1; x++) {
            double d = sdfRoundRect(x + 0.5, y + 0.5, x0, y0, x1, y1, rr);
            double a = clampd(0.5 - d / SOFT_PX, 0.0, 1.0) * alpha;
            if (a <= 0) continue;
            BlendPM(dst, pw, ph, x, y, cr, cg, cb, a);
        }
}

/* ------------------------------------------------- 形状主渲染（SSAA） */
static void RenderIsland(DWORD *dst, int pw, int ph,
                         double cx, double cy, double RX, double RY, double r)
{
    int cr = GetRValue(ISLAND_COLOR);
    int cg = GetGValue(ISLAND_COLOR);
    int cb = GetBValue(ISLAND_COLOR);
    double op = (double)ISLAND_OPACITY / 255.0;
    double band = SOFT_PX + BAND_PAD;

    for (int i = 0; i < pw * ph; i++) dst[i] = 0;

    int x0 = (int)floor(cx - RX - band - 2), x1 = (int)ceil(cx + RX + band + 2);
    int y0 = (int)floor(cy - RY - band - 2), y1 = (int)ceil(cy + RY + band + 2);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= pw) x1 = pw - 1;
    if (y1 >= ph) y1 = ph - 1;

    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            double d = sdfRoundRect(x + 0.5, y + 0.5, cx - RX, cy - RY, cx + RX, cy + RY, r);
            double a;
            if (d > band)       a = 0.0;
            else if (d < -band) a = 1.0;
            else {
                double acc = 0;
                for (int j = 0; j < SS; j++) {
                    double sy = y + (j + 0.5) / (double)SS;
                    for (int i = 0; i < SS; i++) {
                        double sx = x + (i + 0.5) / (double)SS;
                        double dd = sdfRoundRect(sx, sy, cx - RX, cy - RY, cx + RX, cy + RY, r);
                        acc += clampd(0.5 - dd / SOFT_PX, 0.0, 1.0);
                    }
                }
                a = acc / (double)(SS * SS);
            }
            a *= op;
            if (a <= 0.0) continue;
            int ai = (int)(a * 255.0 + 0.5);
            if (ai > 255) ai = 255;
            dst[y * pw + x] = ((DWORD)ai << 24) | ((DWORD)((cr * ai) / 255) << 16) |
                              ((DWORD)((cg * ai) / 255) << 8) | (DWORD)((cb * ai) / 255);
        }
    }
}

/* ---------------------------------------------------- 岛内容（静默/音乐） */
static double g_scrollT = 0;

static void DrawIslandContent(DWORD *dst, int pw, int ph,
                              double cx, double cy, double RX, double RY,
                              double s, const Media *m, double alpha)
{
    if (alpha <= 0.001) return;

    if (!m->has) {
        /* 静默：时钟居中 */
        WCHAR t[TEXT_MAX];
        BuildClockText(t, TEXT_MAX);
        RECT rc = {(int)(cx - RX + 10), (int)(cy - RY), (int)(cx + RX - 10), (int)(cy + RY)};
        DrawTextIn(dst, pw, ph, &rc, t,
                   (int)(ISLAND_FSIZE * s * (1.0 - press) + 0.5),
                   ISLAND_FWEIGHT, TEXT_COLOR, 0, alpha);
        return;
    }

    /* 音乐：圆形封面 + 歌名（过长来回滚动） */
    double fs = ISLAND_FSIZE * s * (1.0 - press);
    double coverR = RY - 4.0 * s;
    if (coverR < 2) coverR = 2;
    double coverCx = cx - RX + 6.0 * s + coverR;
    int hasCov = (m->cover && m->coverW > 0 && m->coverH > 0);

    if (hasCov)
        DrawCoverCircle(dst, pw, ph, coverCx, cy, coverR,
                        m->cover, m->coverW, m->coverH, alpha);

    double textL = hasCov ? (coverCx + coverR + 7.0 * s) : (cx - RX + 10.0 * s);
    double textR = cx + RX - 10.0 * s;
    double avail = textR - textL;

    HDC dc = GetDC(NULL);
    double tw = 0;
    if (dc) { tw = TextWidth(dc, m->title, (int)(fs + 0.5), ISLAND_FWEIGHT); ReleaseDC(NULL, dc); }

    double off = 0;
    if (tw > avail && avail > 4) {
        double travel = tw - avail;
        double t1 = travel / SCROLL_SPEED * 1000.0;      /* 单程 ms */
        double cycle = t1 * 2.0 + SCROLL_PAUSE * 2.0;
        double phs = fmod(g_scrollT, cycle);
        if (phs < SCROLL_PAUSE)                 off = 0;
        else if (phs < SCROLL_PAUSE + t1)       off = -(phs - SCROLL_PAUSE) / t1 * travel;
        else if (phs < SCROLL_PAUSE * 2 + t1)   off = -travel;
        else off = -travel + (phs - SCROLL_PAUSE * 2 - t1) / t1 * travel;
    } else {
        g_scrollT = 0;
    }

    /* 裁剪到 [textL, textR] 后按 offset 绘制 */
    RECT rc = {(int)(textL + off), (int)(cy - RY), (int)(textL + off + tw + 4), (int)(cy + RY)};
    DrawTextIn(dst, pw, ph, &rc, m->title, (int)(fs + 0.5), ISLAND_FWEIGHT,
               TEXT_COLOR, 1, alpha);
}

/* ---------------------------------------------------- 卡片内容（播放器） */
/* 按钮命中区（内容坐标，相对窗口左上角，设备像素） */
typedef struct { double x0, y0, x1, y1; int id; } HitBox;
#define HIT_NONE 0
#define HIT_PREV 1
#define HIT_PLAY 2
#define HIT_NEXT 3
#define HIT_BAR  4

static HitBox g_boxes[4];
static int    g_nbox = 0;
static int    g_hoverBox = HIT_NONE;

static void CardLayout(double s, double cx, double cy, double RX, double RY,
                       double *covX, double *covY, double *covR)
{
    double pad = 18.0 * s;
    double side = 96.0 * s;
    if (side > 2.0 * RY - 2.0 * pad) side = 2.0 * RY - 2.0 * pad;
    if (side < 8) side = 8;
    *covR  = side / 2.0;
    *covX  = cx - RX + pad + *covR;
    *covY  = cy - RY + pad + *covR;
}

static void DrawCardContent(DWORD *dst, int pw, int ph,
                            double cx, double cy, double RX, double RY,
                            double s, const Media *m, double alpha)
{
    g_nbox = 0;
    if (alpha <= 0.001) return;

    double covX, covY, covR;
    CardLayout(s, cx, cy, RX, RY, &covX, &covY, &covR);

    /* 封面（拿不到就画深灰占位块，保持布局） */
    if (m->cover && m->coverW > 0 && m->coverH > 0)
        DrawCoverCircle(dst, pw, ph, covX, covY, covR, m->cover, m->coverW, m->coverH, alpha);
    else
        FillRoundRect(dst, pw, ph, covX - covR, covY - covR, covX + covR, covY + covR,
                      covR, 42, 42, 46, alpha);

    /* 右侧文字 */
    double tl = covX + covR + 16.0 * s;
    double tr = cx + RX - 18.0 * s;
    double ty = covY - covR + 4.0 * s;

    RECT rcT = {(int)tl, (int)ty, (int)tr, (int)(ty + 26 * s)};
    DrawTextIn(dst, pw, ph, &rcT, m->title,
               (int)(CARD_TITLE_FS * s + 0.5), FW_BOLD, TEXT_COLOR, 1, alpha);

    RECT rcA = {(int)tl, (int)(ty + 30 * s), (int)tr, (int)(ty + 52 * s)};
    DrawTextIn(dst, pw, ph, &rcA, m->artist,
               (int)(CARD_ARTIST_FS * s + 0.5), FW_NORMAL, SUB_TEXT_COLOR, 1, alpha);

    /* 三个按钮 */
    double bd = 46.0 * s;
    double by = covY + covR + 16.0 * s;
    double gap = 22.0 * s;
    double total = bd * 3 + gap * 2;
    double bx0 = cx - total / 2.0;
    int ids[3] = {HIT_PREV, HIT_PLAY, HIT_NEXT};
    for (int i = 0; i < 3; i++) {
        double x0 = bx0 + i * (bd + gap);
        double x1 = x0 + bd;
        double y0 = by, y1 = by + bd;
        int hover = (g_hoverBox == ids[i]);
        int cr = hover ? GetRValue(BTN_HOVER) : GetRValue(BTN_COLOR);
        int cg = hover ? GetGValue(BTN_HOVER) : GetGValue(BTN_COLOR);
        int cb = hover ? GetBValue(BTN_HOVER) : GetBValue(BTN_COLOR);
        FillRoundRect(dst, pw, ph, x0, y0, x1, y1, bd / 2.0, cr, cg, cb, alpha);

        /* 图标 */
        double mx = (x0 + x1) / 2.0, my = (y0 + y1) / 2.0;
        int w = 255;
        if (ids[i] == HIT_PREV || ids[i] == HIT_NEXT) {
            /* 三角 + 竖条 */
            double d = 5.0 * s, dir = (ids[i] == HIT_PREV) ? -1.0 : 1.0;
            double tx = mx + dir * 3.0 * s;
            for (double yy = -7.0 * s; yy <= 7.0 * s; yy += 0.5)
                for (double xx = 0; xx <= 7.0 * s; xx += 0.5) {
                    double halfw = 7.0 * s - xx;
                    if (yy >= -halfw && yy <= halfw)
                        BlendPM(dst, pw, ph, (int)(tx + dir * xx), (int)(my + yy), w, w, w, alpha);
                }
            FillRoundRect(dst, pw, ph, tx - dir * 7.0 * s - 2.0 * s, my - 7.0 * s,
                          tx - dir * 7.0 * s, my + 7.0 * s, 1.0, w, w, w, alpha);
            (void)d;
        } else {
            /* 播放 / 暂停 */
            if (m->playing) {
                FillRoundRect(dst, pw, ph, mx - 6.0 * s, my - 7.0 * s, mx - 2.0 * s, my + 7.0 * s,
                              1.0, w, w, w, alpha);
                FillRoundRect(dst, pw, ph, mx + 2.0 * s, my - 7.0 * s, mx + 6.0 * s, my + 7.0 * s,
                              1.0, w, w, w, alpha);
            } else {
                for (double yy = -7.0 * s; yy <= 7.0 * s; yy += 0.5)
                    for (double xx = 0; xx <= 8.0 * s; xx += 0.5) {
                        double halfw = 8.0 * s - xx;
                        if (yy >= -halfw && yy <= halfw)
                            BlendPM(dst, pw, ph, (int)(mx - 4.0 * s + xx), (int)(my + yy), w, w, w, alpha);
                    }
            }
        }
        g_boxes[g_nbox].x0 = x0; g_boxes[g_nbox].y0 = y0;
        g_boxes[g_nbox].x1 = x1; g_boxes[g_nbox].y1 = y1;
        g_boxes[g_nbox].id = ids[i];
        g_nbox++;
    }

    /* 进度条 */
    double barY = by + bd + 22.0 * s;
    double barX0 = cx - RX + 18.0 * s, barX1 = cx + RX - 18.0 * s;
    if (barY + 6.0 * s < cy + RY) {
        double barH = 4.0 * s;
        FillRoundRect(dst, pw, ph, barX0, barY, barX1, barY + barH, barH / 2.0,
                      GetRValue(BAR_COLOR), GetGValue(BAR_COLOR), GetBValue(BAR_COLOR), alpha);
        double dur = m->dur, pos = MediaPosNow(m);
        double frac = (dur > 0) ? clampd(pos / dur, 0.0, 1.0) : 0.0;
        if (frac > 0.001) {
            double fx = barX0 + (barX1 - barX0) * frac;
            FillRoundRect(dst, pw, ph, barX0, barY, fx, barY + barH, barH / 2.0,
                          GetRValue(BAR_FILL), GetGValue(BAR_FILL), GetBValue(BAR_FILL), alpha);
        }
        /* 时间 */
        WCHAR ts[32], te[32];
        int cp = (int)(pos + 0.5), ce = (int)(dur + 0.5);
        _snwprintf(ts, 32, L"%d:%02d", cp / 60, cp % 60);
        _snwprintf(te, 32, L"%d:%02d", ce / 60, ce % 60);
        RECT rc1 = {(int)barX0, (int)(barY + 8 * s), (int)(barX0 + 60 * s), (int)(barY + 24 * s)};
        RECT rc2 = {(int)(barX1 - 60 * s), (int)(barY + 8 * s), (int)barX1, (int)(barY + 24 * s)};
        DrawTextIn(dst, pw, ph, &rc1, ts, (int)(11 * s + 0.5), FW_NORMAL, SUB_TEXT_COLOR, 1, alpha);
        DrawTextIn(dst, pw, ph, &rc2, te, (int)(11 * s + 0.5), FW_NORMAL, SUB_TEXT_COLOR, 0, alpha);

        g_boxes[g_nbox].x0 = barX0 - 4 * s; g_boxes[g_nbox].y0 = barY - 8 * s;
        g_boxes[g_nbox].x1 = barX1 + 4 * s; g_boxes[g_nbox].y1 = barY + barH + 8 * s;
        g_boxes[g_nbox].id = HIT_BAR;
        g_nbox++;
    }
}

/* --------------------------------------------------------------- 渲染 */
static void ApplyIsland(void)
{
    if (!g_hwnd) return;
    if (!g_visible) { ShowWindow(g_hwnd, SW_HIDE); return; }

    RECT   scr;
    double s;
    GetScreen(&scr, &s);
    g_scale = s;

    int bigWin = (g_expanded || expP > 0.01);
    double baseW = bigWin ? CARD_W : ISLAND_W;
    double baseH = bigWin ? CARD_H : ISLAND_H;

    int padX = (int)(PADX * s + 0.5), padY = (int)(PADY * s + 0.5);
    int pw = (int)(baseW * s + 0.5) + 2 * padX;
    int ph = (int)(baseH * s + 0.5) + 2 * padY;
    if (pw < 1) pw = 1;
    if (ph < 1) ph = 1;

    double RX, RY, r;
    shape_geom(s, expP, &RX, &RY, &r);
    double cx = pw / 2.0, cy = padY + RY;

    g_cx = cx; g_cy = cy; g_RX = RX; g_RY = RY; g_r = r;

    Media m;
    MediaSnapshot(&m);

    double clockAlpha = clampd(1.0 - expP / CLOCK_FADE_T, 0.0, 1.0);
    double cardAlpha  = clampd((expP - 0.55) / 0.45, 0.0, 1.0);

    HDC hdc = GetDC(NULL);
    if (!hdc) return;

    BITMAPINFO bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = pw; bi.bmiHeader.biHeight = -ph;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;

    void *bits = NULL;
    HBITMAP dib = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!dib || !bits) { if (dib) DeleteObject(dib); ReleaseDC(NULL, hdc); return; }

    RenderIsland((DWORD *)bits, pw, ph, cx, cy, RX, RY, r);

    EnterCriticalSection(&g_mediaCS);
    unsigned char *cov = g_media.cover;
    int cw = g_media.coverW, chh = g_media.coverH;
    LeaveCriticalSection(&g_mediaCS);
    m.cover = cov; m.coverW = cw; m.coverH = chh;

    if (g_demo && !m.has) {
        m.has = 1; m.playing = 1;
        wcscpy(m.title, L"如果当时");
        wcscpy(m.artist, L"许嵩");
        m.dur = 245; m.pos = 82; m.stamp = now_ms();
    }

    if (clockAlpha > 0.001)
        DrawIslandContent((DWORD *)bits, pw, ph, cx, cy, RX, RY, s, &m, clockAlpha);
    if (cardAlpha > 0.001 && m.has)
        DrawCardContent((DWORD *)bits, pw, ph, cx, cy, RX, RY, s, &m, cardAlpha);

    HDC mem = CreateCompatibleDC(hdc);
    HGDIOBJ old = SelectObject(mem, dib);

    POINT ptSrc = {0, 0};
    POINT ptDst;
    ptDst.x = scr.left + (int)(((scr.right - scr.left) - pw) / 2);
    ptDst.y = scr.top + (int)(ISLAND_INSET * s + 0.5) - padY;
    SIZE sz = {pw, ph};

    BLENDFUNCTION bf;
    bf.BlendOp = AC_SRC_OVER; bf.BlendFlags = 0;
    bf.SourceConstantAlpha = 255; bf.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(g_hwnd, NULL, &ptDst, &sz, mem, &ptSrc, 0, &bf, ULW_ALPHA);

    SelectObject(mem, old);
    DeleteDC(mem);
    DeleteObject(dib);
    ReleaseDC(NULL, hdc);

    g_px = ptDst.x; g_py = ptDst.y; g_pw = pw; g_ph = ph;

    ShowWindow(g_hwnd, SW_SHOWNOACTIVATE);
    SetWindowPos(g_hwnd, HWND_TOPMOST, ptDst.x, ptDst.y, pw, ph,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

/* -------------------------------------------------------- 命中测试 */
static int HitShape(int lx, int ly)
{
    if (!g_visible || g_RX <= 0 || g_RY <= 0) return 0;
    return sdfRoundRect(lx + 0.5, ly + 0.5,
                        g_cx - g_RX, g_cy - g_RY,
                        g_cx + g_RX, g_cy + g_RY, g_r) < 0.0;
}

static int HitBoxAt(int lx, int ly)
{
    if (clampd(expP, 0, 1) < 0.9) return HIT_NONE;
    for (int i = 0; i < g_nbox; i++)
        if (lx >= g_boxes[i].x0 && lx <= g_boxes[i].x1 &&
            ly >= g_boxes[i].y0 && ly <= g_boxes[i].y1)
            return g_boxes[i].id;
    return HIT_NONE;
}

/* ------------------------------------------------- 点击卡片外收起 */
static LRESULT CALLBACK MouseHookLL(int nCode, WPARAM wp, LPARAM lp)
{
    if (nCode >= 0 && g_hwnd && g_visible && g_expanded) {
        if (wp == WM_LBUTTONDOWN || wp == WM_NCLBUTTONDOWN ||
            wp == WM_RBUTTONDOWN || wp == WM_NCRBUTTONDOWN ||
            wp == WM_MBUTTONDOWN || wp == WM_NCMBUTTONDOWN) {
            MSLLHOOKSTRUCT *hs = (MSLLHOOKSTRUCT *)lp;
            int lx = hs->pt.x - g_px, ly = hs->pt.y - g_py;
            if (!HitShape(lx, ly))
                PostMessageW(g_hwnd, WM_COLLAPSE, 0, 0);
        }
    }
    return CallNextHookEx(g_hook, nCode, wp, lp);
}

/* -------------------------------------------------------- 交互动作 */
static void ToggleVisible(void)
{
    g_visible = !g_visible;
    if (!g_visible) g_expanded = 0;
    ApplyIsland();
}

static void ToggleExpand(void)
{
    g_expanded = !g_expanded;
    if (!g_anim) { g_anim = 1; SetTimer(g_hwnd, TIMER_TICK, 16, NULL); }
    ApplyIsland();
}

static void Kick(void)
{
    popUntil = now_ms() + POP_HOLD_MS;
    if (!g_anim) { g_anim = 1; SetTimer(g_hwnd, TIMER_TICK, 16, NULL); }
}

static void ShowMenu(void)
{
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, IDM_EXPAND, g_expanded ? L"收起卡片" : L"展开卡片");
    AppendMenuW(m, MF_STRING, IDM_TOGGLE, g_visible ? L"隐藏" : L"显示");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, IDM_EXIT, L"退出");
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(g_hwnd);
    TrackPopupMenu(m, TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_hwnd, NULL);
    PostMessageW(g_hwnd, WM_NULL, 0, 0);
    DestroyMenu(m);
}

/* -------------------------------------------------------- 窗口过程 */
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        g_hwnd = hwnd;
        g_lastT = now_ms();
        RegisterHotKey(hwnd, HOTKEY_ID, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'N');
        ApplyIsland();
        SetTimer(hwnd, TIMER_TICK, 250, NULL);
        g_hook = SetWindowsHookExW(WH_MOUSE_LL, MouseHookLL, NULL, 0);
        return 0;

    case WM_WAKE:
        if (!g_visible) ToggleVisible();
        return 0;

    case WM_HOTKEY:
        if (wp == HOTKEY_ID) ToggleVisible();
        return 0;

    case WM_COLLAPSE:
        if (g_expanded) ToggleExpand();
        return 0;

    case WM_NCHITTEST: {
        int lx = (short)LOWORD(lp) - g_px;
        int ly = (short)HIWORD(lp) - g_py;
        return HitShape(lx, ly) ? HTCLIENT : HTTRANSPARENT;
    }

    case WM_LBUTTONDOWN:
        holding = 1;
        Kick();
        g_lastT = now_ms();
        ApplyIsland();
        return 0;

    case WM_LBUTTONUP: {
        holding = 0;
        int lx = (short)LOWORD(lp) - g_px;
        int ly = (short)HIWORD(lp) - g_py;
        int box = HitBoxAt(lx, ly);
        if (box == HIT_PREV)     DoCommand(1, 0);
        else if (box == HIT_PLAY) DoCommand(2, 0);
        else if (box == HIT_NEXT) DoCommand(3, 0);
        else if (box == HIT_BAR) {
            Media m; MediaSnapshot(&m);
            if (m.dur > 0) {
                double frac = clampd((lx - g_boxes[g_nbox - 1].x0) /
                                     (g_boxes[g_nbox - 1].x1 - g_boxes[g_nbox - 1].x0), 0.0, 1.0);
                DoCommand(4, frac * m.dur);
            }
        }
        return 0;
    }

    case WM_LBUTTONDBLCLK: {
        /* 双击按钮/进度条不切换展开，避免误触 */
        int lx = (short)LOWORD(lp) - g_px;
        int ly = (short)HIWORD(lp) - g_py;
        if (HitBoxAt(lx, ly) == HIT_NONE) ToggleExpand();
        return 0;
    }

    case WM_RBUTTONUP:
        ShowMenu();
        return 0;

    case WM_TIMER: {
                double now = now_ms();
        double dt = now - g_lastT;
        g_lastT = now;
        if (dt < 0) dt = 0;
        if (dt > 100) dt = 100;

        g_scrollT += dt;

        /* 按钮 hover */
        if (g_expanded) {
            POINT pt;
            if (GetCursorPos(&pt)) {
                int hb = HitBoxAt(pt.x - g_px, pt.y - g_py);
                if (hb != g_hoverBox) { g_hoverBox = hb; ApplyIsland(); }
            }
        }

                if (anim_active(now)) {
            step(now, dt);
            if (fabs(press) < 2e-4 && fabs(pressV) < 2e-3) { press = 0; pressV = 0; sq = 0; }
            if (fabs(expP - (g_expanded ? 1.0 : 0.0)) < 2e-4 && fabs(expV) < 2e-3)
                { expP = g_expanded ? 1.0 : 0.0; expV = 0; }
            if (g_hoverBox == HIT_NONE) ApplyIsland();
        } else {
            if (g_anim) { g_anim = 0; SetTimer(hwnd, TIMER_TICK, 250, NULL); }
            /* 时钟 / 滚动 / 进度 需要持续刷新 */
                        Media m; MediaSnapshot(&m);
                        int need = 0;
            if (!m.has && g_mode != TM_TEXT) {
                WCHAR t[TEXT_MAX]; BuildClockText(t, TEXT_MAX);
                if (wcscmp(t, g_lastTxt) != 0) { wcscpy(g_lastTxt, t); need = 1; }
            } else if (m.has) need = 1;      /* 滚动 + 进度 */
                        if (need && g_hoverBox == HIT_NONE) ApplyIsland();
                    }
        return 0;
    }

    case WM_COMMAND:
        if (LOWORD(wp) == IDM_EXPAND) ToggleExpand();
        else if (LOWORD(wp) == IDM_TOGGLE) ToggleVisible();
        else if (LOWORD(wp) == IDM_EXIT) PostMessageW(hwnd, WM_CLOSE, 0, 0);
        return 0;

    case WM_DISPLAYCHANGE:
    case WM_SETTINGCHANGE:
        ApplyIsland();
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, TIMER_TICK);
        if (g_hook) { UnhookWindowsHookEx(g_hook); g_hook = NULL; }
        StopCollector();
        UnregisterHotKey(hwnd, HOTKEY_ID);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* ------------------------------------------------------------ 诊断 */
static void RunDiag(void)
{
    ScanWindows();                       /* 立刻扫一遍，拿最新窗口列表 */

    Media m;
    MediaSnapshot(&m);

    WCHAR *osName = L"未知";
    if (g_osMajor == 10 && g_osBuild >= 22000)      osName = L"Windows 11";
    else if (g_osMajor == 10)                       osName = L"Windows 10";
    else if (g_osMajor == 6 && g_osMinor == 3)      osName = L"Windows 8.1";
    else if (g_osMajor == 6 && g_osMinor == 2)      osName = L"Windows 8";
    else if (g_osMajor == 6 && g_osMinor == 1)      osName = L"Windows 7";
    else if (g_osMajor == 6 && g_osMinor == 0)      osName = L"Windows Vista";

    const WCHAR *srcName = (m.src == 1) ? L"SMTC 系统媒体会话（完整：封面/进度/控制）"
                         : (m.src == 2) ? L"窗口标题法（后备：只有歌名歌手）"
                         : L"未识别到任何正在播放的媒体";

    WCHAR msg[4096];
    int o = 0;
    o += _snwprintf(msg + o, 4096 - o,
        L"QIsland 媒体诊断\n"
        L"────────────────────────\n"
        L"系统：%s  (版本 %d.%d, build %d)\n"
        L"SMTC 支持：%s\n"
        L"当前数据源：%s\n\n",
        osName, g_osMajor, g_osMinor, g_osBuild,
        SMTC_Available() ? L"可用" : L"不可用（需 Windows 10 1809 / build 17763+）",
        srcName);

    if (m.has)
        o += _snwprintf(msg + o, 4096 - o,
            L"识别结果：\n  歌名：%s\n  歌手：%s\n\n",
            m.title[0] ? m.title : L"(空)", m.artist[0] ? m.artist : L"(空)");

    WinCand cc[MAXCAND];
    int n;
    EnterCriticalSection(&g_candCS);
    n = g_ncand;
    memcpy(cc, g_cands, sizeof(cc));
    LeaveCriticalSection(&g_candCS);

    o += _snwprintf(msg + o, 4096 - o, L"检测到的播放器/浏览器窗口：%d 个\n", n);
    for (int i = 0; i < n && i < 12; i++)
        o += _snwprintf(msg + o, 4096 - o, L"  [%d] %s\n      %s\n",
                        cc[i].prio, cc[i].exe, cc[i].title);
    if (n > 12) o += _snwprintf(msg + o, 4096 - o, L"  …还有 %d 个\n", n - 12);
    if (n == 0) o += _snwprintf(msg + o, 4096 - o, L"  （无）\n");

    o += _snwprintf(msg + o, 4096 - o,
        L"\n────────────────────────\n"
        L"标题法只认「窗口标题里带歌名」的播放器。\n\n"
        L"· 列表为空 → 播放器没在播，或它是 Electron / 自绘\n"
        L"  界面（标题恒为程序名，读不到歌名）。\n"
        L"· 列表有但歌名为空 → 标题格式特殊，把上面那行标题\n"
        L"  原文发我，我加一条解析规则。\n\n"
        L"本诊断已同时写入 %%TEMP%%\\WinIsland_diag.txt");

    WCHAR tmp[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    wcscat(tmp, L"WinIsland_diag.txt");
    HANDLE f = CreateFileW(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (f != INVALID_HANDLE_VALUE) {
        char *u8 = (char *)malloc(8192);
        if (u8) {
            int n8 = WideCharToMultiByte(CP_UTF8, 0, msg, -1, u8, 8192, NULL, NULL);
            DWORD w = 0;
            WriteFile(f, u8, (DWORD)(n8 > 0 ? n8 - 1 : 0), &w, NULL);
            free(u8);
        }
        CloseHandle(f);
    }

    MessageBoxW(NULL, msg, APP_NAME, MB_OK | MB_ICONINFORMATION);
}

/* ============================================================
 *  --probe：把汽水音乐进程里「能读到的东西」全部dump出来
 *  Electron 自绘界面不把歌名放进窗口标题，所以要看：
 *    1) 所有子窗口的文本      2) 本地数据目录里的播放队列/状态文件
 * ========================================================== */
static HANDLE g_pf = NULL;

static void pfput(const char *s)
{
    if (!g_pf) return;
    DWORD w = 0;
    WriteFile(g_pf, s, (DWORD)strlen(s), &w, NULL);
}
static void pfputw(const WCHAR *s)
{
    if (!g_pf || !s) return;
    char b[4096];
    int n = WideCharToMultiByte(CP_UTF8, 0, s, -1, b, 4096, NULL, NULL);
    DWORD w = 0;
    if (n > 1) WriteFile(g_pf, b, (DWORD)(n - 1), &w, NULL);
}
static void pfline(const char *pre, const WCHAR *w, const char *post)
{
    pfput(pre);
    pfputw(w);
    pfput(post);
}
static void pffmt(const char *fmt, ...)
{
    char b[2048];
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf(b, 2048, fmt, ap);
    va_end(ap);
    pfput(b);
}

/* ---- 收集某进程的所有窗口 ---- */
typedef struct { HWND h; int depth; } WHit;
static WHit  g_wh[400];
static int   g_nwh = 0;
static DWORD g_targetPid = 0;
static int   g_enumDepth = 0;

static BOOL CALLBACK ChildProc(HWND h, LPARAM lp)
{
    (void)lp;
    if (g_nwh >= 400) return FALSE;
    g_wh[g_nwh].h = h;
    g_wh[g_nwh].depth = g_enumDepth;
    g_nwh++;
    return TRUE;
}
static BOOL CALLBACK TopProc(HWND h, LPARAM lp)
{
    (void)lp;
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid != g_targetPid) return TRUE;
    if (g_nwh < 400) {
        g_wh[g_nwh].h = h; g_wh[g_nwh].depth = 0; g_nwh++;
    }
    /* 该顶层窗口的所有后代 */
    int base = g_nwh;
    g_enumDepth = 1;
    EnumChildWindows(h, ChildProc, 0);
    (void)base;
    return TRUE;
}

/* ---- 找进程 ---- */
static DWORD FindProcByExeName(const WCHAR *pat, WCHAR *outPath, DWORD cch)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe;
    ZeroMemory(&pe, sizeof(pe));
    pe.dwSize = sizeof(pe);
    DWORD found = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            WCHAR nm[300];
            wcsncpy(nm, pe.szExeFile, 299); nm[299] = 0;
            wlower(nm);
            WCHAR p[64]; wcsncpy(p, pat, 63); p[63] = 0; wlower(p);
            if (wcsstr(nm, p)) { found = pe.th32ProcessID; break; }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    if (!found || !outPath) return found;

    outPath[0] = 0;
    HANDLE ms = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, found);
    if (ms != INVALID_HANDLE_VALUE) {
        MODULEENTRY32W me;
        ZeroMemory(&me, sizeof(me));
        me.dwSize = sizeof(me);
        if (Module32FirstW(ms, &me))
            wcsncpy(outPath, me.szExePath, cch - 1);
        CloseHandle(ms);
    }
    return found;
}

/* ---- 目录扫描 ---- */
typedef struct { WCHAR path[MAX_PATH]; WCHAR name[160]; DWORD size; FILETIME ft; int depth; } FEnt;
static FEnt g_files[4000];
static int  g_nfiles = 0;

static void ScanDir(const WCHAR *dir, int depth)
{
    if (depth > 3 || g_nfiles >= 4000) return;
    WCHAR pat[MAX_PATH];
    _snwprintf(pat, MAX_PATH, L"%s\\*", dir);
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
        WCHAR full[MAX_PATH];
        _snwprintf(full, MAX_PATH, L"%s\\%s", dir, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            ScanDir(full, depth + 1);
            continue;
        }
        if (g_nfiles < 4000) {
            wcsncpy(g_files[g_nfiles].path, full, MAX_PATH - 1);
            wcsncpy(g_files[g_nfiles].name, fd.cFileName, 159);
            g_files[g_nfiles].name[159] = 0;
            g_files[g_nfiles].size = fd.nFileSizeLow;
            g_files[g_nfiles].ft = fd.ftLastWriteTime;
            g_files[g_nfiles].depth = depth;
            g_nfiles++;
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

/* 可打印串提取：ASCII / UTF-8 */
static void DumpPrintable(const unsigned char *b, int n)
{
    int run = 0, start = 0;
    for (int i = 0; i <= n; i++) {
        int okc = 0;
        if (i < n) {
            unsigned char c = b[i];
            okc = (c >= 0x20 && c < 0x7F) || c == '\n' || c == '\r' || c == '\t' || c >= 0x80;
        }
        if (okc) {
            if (!run) start = i;
            run++;
        } else {
            if (run >= 5) {
                char *seg = (char *)malloc(run + 1);
                if (seg) {
                    memcpy(seg, b + start, run);
                    seg[run] = 0;
                    /* 转成 UTF-8 输出（原串按本地编码，这里做安全替换） */
                    int wlen = MultiByteToWideChar(CP_UTF8, 0, seg, -1, NULL, 0);
                    if (wlen > 1) {
                        WCHAR *w = (WCHAR *)malloc(wlen * sizeof(WCHAR));
                        if (w) {
                            MultiByteToWideChar(CP_UTF8, 0, seg, -1, w, wlen);
                            pfput("    "); pfputw(w); pfput("\n");
                            free(w);
                        }
                    } else {
                        WCHAR *w = (WCHAR *)malloc((run + 1) * sizeof(WCHAR));
                        if (w) {
                            for (int k = 0; k < run; k++) w[k] = (WCHAR)b[start + k];
                            w[run] = 0;
                            pfput("    "); pfputw(w); pfput("\n");
                            free(w);
                        }
                    }
                    free(seg);
                }
            }
            run = 0;
        }
    }
}

/* UTF-16LE 串提取 */
static void DumpUTF16(const unsigned char *b, int n)
{
    for (int i = 0; i + 1 < n; i += 2) {
        int j = i, cnt = 0;
        while (j + 1 < n) {
            unsigned int c = (unsigned int)b[j] | ((unsigned int)b[j + 1] << 8);
            int okc = (c >= 0x20 && c < 0x7F) || c == 0x0A || c == 0x0D || c == 0x09 || c >= 0x4E00;
            if (!okc) break;
            j += 2; cnt++;
            if (cnt >= 200) break;
        }
        if (cnt >= 6) {
            int good = 0;
            for (int t = 0; t < cnt; t++) {
                unsigned int c = (unsigned int)b[i + t * 2] | ((unsigned int)b[i + t * 2 + 1] << 8);
                if ((c >= 0x20 && c < 0x7F) || (c >= 0x4E00 && c <= 0x9FFF)) good++;
            }
            if (good * 5 < cnt * 3) continue;   /* 乱码太多，跳过 */
            WCHAR *w = (WCHAR *)malloc((cnt + 1) * sizeof(WCHAR));
            if (w) {
                memcpy(w, b + i, cnt * 2);
                w[cnt] = 0;
                pfput("    "); pfputw(w); pfput("\n");
                free(w);
            }
            i = j - 2;   /* for 循环还会 +2 */
        }
    }
}

static void DumpFile(const WCHAR *path, const char *label)
{
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) { pffmt("  [打不开] %ls\n", path); return; }
    DWORD sz = GetFileSize(f, NULL);
    if (sz == INVALID_FILE_SIZE || sz == 0) { CloseHandle(f); return; }
    DWORD want = sz > 3072 ? 3072 : sz;
    unsigned char *b = (unsigned char *)malloc(want);
    if (!b) { CloseHandle(f); return; }
    DWORD rd = 0;
    ReadFile(f, b, want, &rd, NULL);
    CloseHandle(f);

    pfline("\n===== 文件: ", path, " =====\n");
    pffmt("  %s  大小=%lu 字节  已读取前 %lu 字节\n", label, (unsigned long)sz, (unsigned long)rd);

    pfput("  --- UTF-8 / 可打印串 ---\n");
    DumpPrintable(b, (int)rd);
    pfput("  --- UTF-16LE 串 ---\n");
    DumpUTF16(b, (int)rd);
    pfput("  --- 前 256 字节 hex ---\n    ");
    for (int i = 0; i < (int)rd && i < 256; i++) {
        pffmt("%02X ", b[i]);
        if ((i % 16) == 15) pfput("\n    ");
    }
    pfput("\n");
    free(b);
}

static void RunProbe(void)
{
    /* 输出文件放桌面，找不到就 %TEMP% */
    WCHAR out[MAX_PATH];
    out[0] = 0;
    /* 桌面优先（好找），失败退到 %TEMP% */
    if (SHGetFolderPathW(NULL, CSIDL_DESKTOP, NULL, SHGFP_TYPE_CURRENT, out) != S_OK &&
        SHGetFolderPathW(NULL, CSIDL_DESKTOP, NULL, SHGFP_TYPE_DEFAULT, out) != S_OK)
        GetTempPathW(MAX_PATH, out);
    wcscat(out, L"\\WinIsland_probe.txt");

    g_pf = CreateFileW(out, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL, NULL);
    if (g_pf == INVALID_HANDLE_VALUE) {
        MessageBoxW(NULL, L"无法创建探测报告文件", APP_NAME, MB_OK | MB_ICONERROR);
        return;
    }

    DetectOS();
    pfput("QIsland 探测报告\n");
    pffmt("系统: %d.%d build %d   SMTC: %s\n\n",
          g_osMajor, g_osMinor, g_osBuild,
          SMTC_Available() ? "可用" : "不可用");

    /* ---- 1. 进程与窗口 ---- */
    pfput("==================== 进程 / 窗口 ====================\n");
    WCHAR exePath[MAX_PATH];
    DWORD pid = FindProcByExeName(L"sodamusic", exePath, MAX_PATH);
    if (!pid) pid = FindProcByExeName(L"qishui", exePath, MAX_PATH);
    if (!pid) pid = FindProcByExeName(L"luna", exePath, MAX_PATH);

    if (!pid) {
        pfput("未找到汽水音乐进程（SodaMusic.exe）。请先启动并播放一首歌。\n");
    } else {
        pffmt("进程 pid=%lu\n路径: %ls\n\n", (unsigned long)pid, exePath);
        g_nwh = 0;
        g_targetPid = pid;
        EnumWindows(TopProc, 0);
        pffmt("窗口总数: %d （depth 0=顶层, >0=子窗口）\n\n", g_nwh);
        for (int i = 0; i < g_nwh && i < 300; i++) {
            WCHAR cls[128], txt[512];
            cls[0] = 0; txt[0] = 0;
            GetClassNameW(g_wh[i].h, cls, 128);
            GetWindowTextW(g_wh[i].h, txt, 512);
            RECT r;
            GetWindowRect(g_wh[i].h, &r);
            pffmt("[%2d] d=%d hwnd=0x%p class=%ls\n",
                  i, g_wh[i].depth, (void *)g_wh[i].h, cls);
            pffmt("     title=\"%ls\"\n", txt[0] ? txt : L"");
            pffmt("     rect=(%ld,%ld)-(%ld,%ld) %ldx%ld visible=%d\n",
                  r.left, r.top, r.right, r.bottom, r.right - r.left, r.bottom - r.top,
                  IsWindowVisible(g_wh[i].h) ? 1 : 0);
        }
        pfput("\n说明: 若所有 title 都为空，说明是自绘界面，只能走本地文件。\n");
    }

    /* ---- 2. 本地数据目录 ---- */
    pfput("\n==================== 本地数据目录 ====================\n");
    WCHAR roots[8][MAX_PATH];
    int nroot = 0;
    ZeroMemory(roots, sizeof(roots));
    const WCHAR *names[] = { L"SodaMusic", L"LunaMusic", L"luna_pc", L"QishuiMusic", L"qishui" };
    WCHAR appd[MAX_PATH], locd[MAX_PATH];
    if (GetEnvironmentVariableW(L"APPDATA", appd, MAX_PATH)) {
        for (int i = 0; i < 5 && nroot < 8; i++) {
            _snwprintf(roots[nroot], MAX_PATH, L"%s\\%s", appd, names[i]);
            nroot++;
        }
    }
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", locd, MAX_PATH)) {
        for (int i = 0; i < 2 && nroot < 8; i++) {
            _snwprintf(roots[nroot], MAX_PATH, L"%s\\%s", locd, names[i]);
            nroot++;
        }
    }

    g_nfiles = 0;
    for (int i = 0; i < nroot; i++) {
        DWORD a = GetFileAttributesW(roots[i]);
        if (a == INVALID_FILE_ATTRIBUTES || !(a & FILE_ATTRIBUTE_DIRECTORY)) {
            pffmt("[不存在] %ls\n", roots[i][0] ? roots[i] : L"(空)");
            continue;
        }
        pffmt("[存在]   %ls\n", roots[i][0] ? roots[i] : L"(空)");
        ScanDir(roots[i], 0);
    }
    pffmt("\n共扫到 %d 个文件（深度<=3）\n", g_nfiles);

    /* 按修改时间排序，最新的在前 */
    for (int i = 1; i < g_nfiles; i++) {
        FEnt k = g_files[i];
        int j = i - 1;
        while (j >= 0) {
            __int64 a = ((__int64)g_files[j].ft.dwHighDateTime << 32) | g_files[j].ft.dwLowDateTime;
            __int64 b = ((__int64)k.ft.dwHighDateTime << 32) | k.ft.dwLowDateTime;
            if (a >= b) break;
            g_files[j + 1] = g_files[j];
            j--;
        }
        g_files[j + 1] = k;
    }

    pfput("\n---- 最近修改的 40 个文件 ----\n");
    for (int i = 0; i < g_nfiles && i < 40; i++) {
        SYSTEMTIME st;
        FILETIME lf;
        FileTimeToLocalFileTime(&g_files[i].ft, &lf);
        FileTimeToSystemTime(&lf, &st);
        pffmt("  %02d-%02d %02d:%02d:%02d  %8lu  %ls\n",
              st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
              (unsigned long)g_files[i].size, g_files[i].path);
    }

    /* ---- 3. 关键文件内容 ---- */
    pfput("\n==================== 候选文件内容 ====================\n");
    const WCHAR *keys[] = { L"queue", L"play", L"current", L"state", L"storage",
                            L"recent", L"history", L"now", L"song", L"music",
                            L"session", L"lyric", L"lrc", L"index" };
    int dumped = 0;
    for (int i = 0; i < g_nfiles && dumped < 6; i++) {
        WCHAR nm[200];
        wcsncpy(nm, g_files[i].name, 199); nm[199] = 0; wlower(nm);
        int hit = 0;
        for (int k = 0; k < (int)(sizeof(keys)/sizeof(keys[0])); k++) {
            WCHAR kk[64]; wcsncpy(kk, keys[k], 63); kk[63] = 0; wlower(kk);
            if (wcsstr(nm, kk)) { hit = 1; break; }
        }
        if (!hit) continue;
        if (g_files[i].size == 0 || g_files[i].size > 4u * 1024 * 1024) continue;
        DumpFile(g_files[i].path, "按关键字命中");
        dumped++;
    }
    if (!dumped)
        pfput("  没有文件名含关键字的候选文件，下面直接看最新的几个小文件。\n");
    for (int i = 0; i < g_nfiles && dumped < 8; i++) {
        if (g_files[i].size == 0 || g_files[i].size > 256u * 1024) continue;
        if (i < 6) continue;              /* 前面已 dump 过的跳过 */
        DumpFile(g_files[i].path, "最新小文件");
        dumped++;
        if (dumped >= 8) break;
    }

    /* ---- 4. LUNA 容器解压结果（最关键） ---- */
    pfput("\n==================== LUNA 队列解压结果 ====================\n");
    {
        WCHAR ad[MAX_PATH];
        if (GetEnvironmentVariableW(L"APPDATA", ad, MAX_PATH)) {
            const WCHAR *lf[] = { L"QueueCache", L"Config", L"AppSettingsCache", L"Status" };
            for (int i = 0; i < 4; i++) {
                WCHAR fp[MAX_PATH];
                _snwprintf(fp, MAX_PATH, L"%s\\SodaMusic\\LunaStorage\\%s", ad, lf[i]);
                if (GetFileAttributesW(fp) == INVALID_FILE_ATTRIBUTES) continue;
                size_t n = 0;
                unsigned char *tx = LunaRead(fp, &n);
                pfline("\n----- ", lf[i], " -----\n");
                if (!tx) { pfput("  解压失败（可能不是 LUNA/gzip 格式）\n"); continue; }
                char h3[64];
                _snprintf(h3, 64, "  解压成功 %d 字节，原文如下：\n", (int)n);
                pfput(h3);
                DWORD w2 = 0;
                DWORD show = (DWORD)(n > 4000 ? 4000 : n);
                WriteFile(g_pf, tx, show, &w2, NULL);
                pfput("\n");
                free(tx);
            }
        }
    }

    pfput("\n==================== 结束 ====================\n");
    pfput("把这份文件发给我，我据此写解析规则。\n");

    FlushFileBuffers(g_pf);
    CloseHandle(g_pf);
    g_pf = NULL;

    WCHAR msg[600];
    _snwprintf(msg, 600,
        L"探测完成。\n\n报告已保存到：\n%ls\n\n"
        L"请把这个文件发给我（直接贴内容即可），\n我据此写针对你这套汽水音乐的解析规则。",
        out);
    MessageBoxW(NULL, msg, APP_NAME, MB_OK | MB_ICONINFORMATION);
}

static void MakeDemoCover(void)
{
    if (g_media.cover) return;
    int n = 200;
    unsigned char *px = (unsigned char *)malloc((size_t)n * n * 4);
    if (!px) return;
    for (int y = 0; y < n; y++) {
        for (int x = 0; x < n; x++) {
            double u = (double)x / n, v = (double)y / n;
            int r = (int)(40 + 200 * u);
            int g = (int)(60 + 120 * v);
            int b = (int)(180 - 120 * u + 60 * v);
            double d = hypot(u - 0.5, v - 0.5);
            if (d < 0.16) { r = 255; g = 255; b = 255; }
            unsigned char *p = px + ((size_t)y * n + x) * 4;
            p[0] = (unsigned char)(b < 0 ? 0 : (b > 255 ? 255 : b));
            p[1] = (unsigned char)(g < 0 ? 0 : (g > 255 ? 255 : g));
            p[2] = (unsigned char)(r < 0 ? 0 : (r > 255 ? 255 : r));
            p[3] = 255;
        }
    }
    EnterCriticalSection(&g_mediaCS);
    g_media.cover = px; g_media.coverW = n; g_media.coverH = n; g_media.coverId++;
    LeaveCriticalSection(&g_mediaCS);
}

static void RenderToBMP(const char *path, int expanded)
{
    g_expanded = expanded;
    expP   = expanded ? 1.0 : 0.0;
    expV   = 0;
    press  = 0; pressV = 0; sq = 0;
    g_hoverBox = HIT_NONE;

    ApplyIsland();                 /* 确定窗口尺寸与几何 */
    int pw = g_pw, ph = g_ph;
    if (pw <= 0 || ph <= 0) { printf("[dump] no geometry\n"); return; }

    double s = g_scale;
    HDC hdc = GetDC(NULL);
    if (!hdc) return;

    BITMAPINFO bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = pw; bi.bmiHeader.biHeight = -ph;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    void *bits = NULL;
    HBITMAP dib = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!dib) { ReleaseDC(NULL, hdc); return; }

    RenderIsland((DWORD *)bits, pw, ph, g_cx, g_cy, g_RX, g_RY, g_r);

    Media m;
    MediaSnapshot(&m);
    EnterCriticalSection(&g_mediaCS);
    m.cover = g_media.cover; m.coverW = g_media.coverW; m.coverH = g_media.coverH;
    LeaveCriticalSection(&g_mediaCS);
    if (g_demo && !m.has) {
        m.has = 1; m.playing = 1;
        wcscpy(m.title, L"如果当时");
        wcscpy(m.artist, L"许嵩");
        m.dur = 245; m.pos = 82; m.stamp = now_ms();
    }

    double clockAlpha = clampd(1.0 - expP / CLOCK_FADE_T, 0.0, 1.0);
    double cardAlpha  = clampd((expP - 0.55) / 0.45, 0.0, 1.0);
    if (clockAlpha > 0.001)
        DrawIslandContent((DWORD *)bits, pw, ph, g_cx, g_cy, g_RX, g_RY, s, &m, clockAlpha);
    if (cardAlpha > 0.001 && m.has)
        DrawCardContent((DWORD *)bits, pw, ph, g_cx, g_cy, g_RX, g_RY, s, &m, cardAlpha);

    /* 合成到浅灰底，另存为 24bpp BMP */
    HANDLE f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (f != INVALID_HANDLE_VALUE) {
        BITMAPFILEHEADER fh; BITMAPINFOHEADER ih;
        ZeroMemory(&fh, sizeof(fh)); ZeroMemory(&ih, sizeof(ih));
        fh.bfType = 0x4D42; fh.bfOffBits = sizeof(fh) + sizeof(ih);
        fh.bfSize = fh.bfOffBits + (DWORD)(pw * ph * 3);
        ih.biSize = sizeof(ih); ih.biWidth = pw; ih.biHeight = -ph;
        ih.biPlanes = 1; ih.biBitCount = 24;
        size_t total = (size_t)pw * ph * 3;
        unsigned char *buf = (unsigned char *)malloc(total);
        if (buf) {
            size_t o = 0;
            for (int y = 0; y < ph; y++)
                for (int x = 0; x < pw; x++) {
                    DWORD p = ((DWORD *)bits)[y * pw + x];
                    int a = (int)((p >> 24) & 0xFF);
                    int r = (int)((p >> 16) & 0xFF);
                    int g = (int)((p >> 8) & 0xFF);
                    int b = (int)(p & 0xFF);
                    double k = 1.0 - a / 255.0;
                    int nr = (int)(r + 226 * k + 0.5);
                    int ng = (int)(g + 228 * k + 0.5);
                    int nb = (int)(b + 232 * k + 0.5);
                    if (nr > 255) nr = 255;
                    if (ng > 255) ng = 255;
                    if (nb > 255) nb = 255;
                    buf[o++] = (unsigned char)nb;
                    buf[o++] = (unsigned char)ng;
                    buf[o++] = (unsigned char)nr;
                }
            DWORD w = 0;
            WriteFile(f, &fh, sizeof(fh), &w, NULL);
            WriteFile(f, &ih, sizeof(ih), &w, NULL);
            WriteFile(f, buf, (DWORD)total, &w, NULL);
            free(buf);
        }
        CloseHandle(f);
        printf("[dump] %s %dx%d\n", path, pw, ph);
    }
    DeleteObject(dib);
    ReleaseDC(NULL, hdc);
}

static void DumpBMP(const char *path)
{
    RenderToBMP(path, 0);

    char card[600];
    _snprintf(card, 600, "%s", path);
    char *dot = strrchr(card, '.');
    if (dot) _snprintf(dot, 600 - (dot - card), "_card.bmp");
    else     strncat(card, "_card.bmp", 600 - strlen(card) - 1);
    RenderToBMP(card, 1);
}

/* ------------------------------------------------------------ 入口 */
static void ParseArgs(int argc, WCHAR **argv)
{
    for (int i = 1; i < argc; i++) {
        WCHAR *a = argv[i];
        if (a[0] != L'-') continue;
        a += (a[1] == L'-') ? 2 : 1;
        if (!_wcsicmp(a, L"clock"))      g_mode = TM_TIME;
        else if (!_wcsicmp(a, L"date"))  g_mode = TM_DATE;
        else if (!_wcsicmp(a, L"hide"))  g_visible = 0;
        else if (!_wcsicmp(a, L"demo"))  g_demo = 1;
        else if (!_wcsicmp(a, L"nocover")) g_noCover = 1;
        else if (!_wcsicmp(a, L"probe"))   g_probe = 1;
        else if (!_wcsicmp(a, L"text") && i + 1 < argc) {
            wcsncpy(g_text, argv[++i], TEXT_MAX - 1);
            g_text[TEXT_MAX - 1] = 0;
            g_mode = TM_TEXT;
        }
    }
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmd, int show)
{
    (void)hPrev; (void)cmd; (void)show;

    if (CreateMutexW(NULL, TRUE, L"WinIsland_SingleInstance") &&
        GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND h = FindWindowW(WND_CLASS, APP_NAME);
        if (h) PostMessageW(h, WM_WAKE, 0, 0);
        return 0;
    }

    InitDpiAware();
    DetectOS();
    InitializeCriticalSection(&g_mediaCS);
    InitializeCriticalSection(&g_candCS);

    int argc = 0;
    WCHAR **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) ParseArgs(argc, argv);

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = WND_CLASS;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE,
        WND_CLASS, APP_NAME, WS_POPUP, 0, 0, 1, 1, NULL, NULL, hInst, NULL);
    if (!hwnd) return 1;

    /* 采集线程 */
    CreateThread(NULL, 0, MediaThread, NULL, 0, NULL);

        /* --diag：等 3 秒让采集跑一轮再弹窗 */
    if (argc > 1) {
        for (int i = 1; i < argc; i++)
            if (!_wcsicmp(argv[i], L"--diag") || !_wcsicmp(argv[i], L"-diag")) {
                Sleep(3000);
                RunDiag();
                StopCollector();
                return 0;
            }
    }

        if (g_probe) {
        RunProbe();
        StopCollector();
        return 0;
    }

    /* --dump：渲染一帧后退出（配合 --demo 生成效果图） */
    for (int i = 1; i + 1 < argc; i++) {
        if (!_wcsicmp(argv[i], L"--dump")) {
            char path[512];
            WideCharToMultiByte(CP_UTF8, 0, argv[i + 1], -1, path, 512, NULL, NULL);
                        Sleep(300);
            if (g_demo && !g_noCover) MakeDemoCover();
            DumpBMP(path);
            StopCollector();
            return 0;
        }
    }

    if (argv) LocalFree(argv);       /* 所有解析结束后才释放 */

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
