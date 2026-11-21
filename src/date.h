/* date.h — 第二张中卡片：带秒的时钟 + 一整组历法日期
 *
 * 文字政策：本页的字符串全部属于「时间 / 日期 / 历法」类，按用户本轮
 * 明确点名登记在 AGENTS.md 第 4 节封闭清单第 9 条；**不是放开文字政策**，
 * 也不得借它给任何别的地方加说明、标签、标题或 placeholder。
 *
 * 模块无状态：不读写文件、不建线程、不装定时器。每次绘制读一次系统本地时间，
 * 全部历法换算由纯函数完成，Win7 原生 API 只有 GetLocalTime。
 * 排版用「目标态标称尺寸」（AGENTS.md 第 4 节第 10 条），文字不会随展开
 * 弹簧的 W/H 每帧重排。
 */
#ifndef QISLAND_DATE_H
#define QISLAND_DATE_H

#include "common.h"
#include "font.h"
#include "canvas.h"

/* W/H 是卡片的目标态标称尺寸（已是设备像素，含 u 缩放）；
   k 是展开淡入系数。画布自身居中，函数内部自行算卡片左端。 */
void date_draw(FontSet *f, Canvas *c, double W, double H, double k);

#endif /* QISLAND_DATE_H */
