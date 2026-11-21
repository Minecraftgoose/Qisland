/* date.h — 第二张中卡片：带秒的时钟 + 一整组历法日期 */
#ifndef QISLAND_DATE_H
#define QISLAND_DATE_H

#include "common.h"
#include "font.h"
#include "canvas.h"

/* W/H 是卡片的目标态标称尺寸（已是设备像素，含 u 缩放）； k 是展开淡入系数。 */
void date_draw(FontSet *f, Canvas *c, double W, double H, double k);

#endif /* QISLAND_DATE_H */
