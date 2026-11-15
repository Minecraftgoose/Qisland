/* tests.h — 内部验证入口（--dump / --diag / --bench / --wheel） */
#ifndef QISLAND_TESTS_H
#define QISLAND_TESTS_H

#include "common.h"

void tests_dump(App *a);
void tests_diag(App *a);
void tests_frame(App *a);       /* 主循环每帧调用 */
int  tests_should_stop(const App *a);
void tests_finish(App *a);      /* 写结果文件 */

#endif /* QISLAND_TESTS_H */
