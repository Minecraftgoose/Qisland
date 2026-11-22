/* version.h — 软件版本的**唯一来源 */
#ifndef QISLAND_VERSION_H
#define QISLAND_VERSION_H

#define QI_VERSION_STR  "0.3.6.2"
#define QI_VERSION_NUM  0,3,6,2      /* VS_FIXEDFILEINFO / VIProductVersion 用 */

#ifndef RC_INVOKED
#define QI_VERSION_W    L"0.3.6.2"    /* 仅 C 代码使用 */
#define QI_VERSION_UI_W L"v" QI_VERSION_W  /* 界面显示带 v；比较与资源仍用纯数字 */
#endif

#endif /* QISLAND_VERSION_H */
