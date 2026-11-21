/* version.h — 软件版本的**唯一来源**
 *
 * 版本号 0.3.2。C 代码和 windres 资源脚本都包含本文件，
 * NSIS 脚本顶部也用同一串字面量（见 src/nsis/Qisland.nsi 的 APPVERSION）。
 * 改版本只改这里 + NSIS 顶部那一处，别在别处硬编码。
 *
 * 本文件只允许放纯宏，不要放类型 / 函数 / C++ 语法：windres 会用 cpp
 * 预处理它，MinGW 的 rc 工具不认宽字符串字面量以外的 C 构造。
 */
#ifndef QISLAND_VERSION_H
#define QISLAND_VERSION_H

#define QI_VERSION_STR  "0.3.2"
#define QI_VERSION_NUM  0,3,2,0      /* VS_FIXEDFILEINFO / VIProductVersion 用 */

#ifndef RC_INVOKED
#define QI_VERSION_W    L"0.3.2"    /* 仅 C 代码使用 */
#endif

#endif /* QISLAND_VERSION_H */
