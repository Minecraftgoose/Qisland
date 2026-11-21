/* tray.h — 系统通知区域（托盘）图标与右键菜单
 *
 * 官方口径（Microsoft Learn / Windows SDK）：
 *   · Shell_NotifyIconW(NIM_ADD) 注册，hWnd 收 uCallbackMessage；
 *   · Win7 上强烈推荐 NIM_SETVERSION + NOTIFYICON_VERSION_4（Vista+）；
 *   · 退出时必须 NIM_DELETE，否则图标会留在托盘直到 explorer 重启；
 *   · 右键菜单：SetForegroundWindow → TrackPopupMenu → PostMessage(WM_NULL)，
 *     否则点击菜单外部不消失、第二次弹出又立刻消失。
 *
 * 主窗口是 WS_EX_NOACTIVATE 的分层窗口，**不能可靠成为前台窗口**，
 * 所以菜单 owner 用常驻的 1x1 屏幕外宿主窗口（tray_init 建、
 * tray_shutdown 销毁），不碰主窗口样式。宿主必须常驻，否则官方要求的
 * PostMessage(WM_NULL) 来不及被线程消息循环取走，第二次弹出会闪退；
 * 另加防重入 + 去抖，吃掉右键连发的 WM_RBUTTONUP / WM_CONTEXTMENU。
 *
 * 菜单两项文字「显示/隐藏灵动岛」「退出」是用户逐字点名的字符串，
 * 登记在 AGENTS.md 封闭清单第 14 条；这是系统外壳菜单，不是岛内 UI，
 * 文字政策未解除。
 */
#ifndef QISLAND_TRAY_H
#define QISLAND_TRAY_H

#include "common.h"

int  tray_init(App *a);
void tray_shutdown(App *a);
void tray_on_message(App *a, WPARAM wp, LPARAM lp);

#endif /* QISLAND_TRAY_H */
