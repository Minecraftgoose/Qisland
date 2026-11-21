/* tray.h — 系统通知区域（托盘）图标与右键菜单 */
#ifndef QISLAND_TRAY_H
#define QISLAND_TRAY_H

#include "common.h"

int  tray_init(App *a);
void tray_shutdown(App *a);
void tray_on_message(App *a, WPARAM wp, LPARAM lp);

#endif /* QISLAND_TRAY_H */
