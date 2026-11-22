/* common.h — 公共头 */
#ifndef QISLAND_COMMON_H
#define QISLAND_COMMON_H

#define WINVER          0x0601
#define _WIN32_WINNT    0x0601
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>       /* GET_X_LPARAM / GET_WHEEL_DELTA_WPARAM */
#include <shellapi.h>
#include <initguid.h>
#include <objbase.h>
#include <oleauto.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <wbemcli.h>
#include <imm.h>            /* IME：便签的中文输入 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct App App;

#ifndef RIDEV_INPUTSINK
#define RIDEV_INPUTSINK 0x00000100
#endif
#ifndef RI_MOUSE_WHEEL
#define RI_MOUSE_WHEEL  0x0400
#endif
#ifndef RI_MOUSE_HWHEEL
#define RI_MOUSE_HWHEEL 0x0800
#endif
#ifndef FR_PRIVATE
#define FR_PRIVATE      0x10
#endif

DWORD qs_now(void);
double qs_clamp(double v, double a, double b);
int    qs_iclamp(int v, int a, int b);
void   qs_clock_w(WCHAR *out, size_t cap);
void   qs_clock_a(char *out, size_t cap);

#endif /* QISLAND_COMMON_H */
