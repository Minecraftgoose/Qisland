/* sys.h — 系统能力：Core Audio 音量、WMI/DDC-CI 亮度、启动任务管理器 */
#ifndef QISLAND_SYS_H
#define QISLAND_SYS_H

#include "common.h"

typedef struct {
    int    comReady;
    IAudioEndpointVolume *epVol;
    int    volOK;
    IWbemServices        *wmiSvc;
    IWbemClassObject     *wmiInSig;
    int    wmiOK;
    HMODULE dxva2;
    int    ddcOK;
    void  *fnGetNum, *fnGetMons, *fnGetBri, *fnSetBri, *fnDestroy;
    double vol;        /* 0..1 */
    double bri;        /* 0..100 */
    DWORD  lastApply;
} Sys;

void sys_init(Sys *s);
void sys_shutdown(Sys *s);
int  sys_set_volume(Sys *s, double f);
int  sys_set_brightness(Sys *s, int pct, HWND hwnd);
void sys_apply_volume(Sys *s, int force);
void sys_apply_brightness(Sys *s, int force, HWND hwnd);
void sys_open_taskmgr(void);

#endif /* QISLAND_SYS_H */
