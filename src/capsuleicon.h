/* capsuleicon.h — 胶囊态右侧白色圆形图标（无倒计时时） */
#ifndef QISLAND_CAPSULEICON_H
#define QISLAND_CAPSULEICON_H
#include "common.h"
#include "canvas.h"     /* Canvas 是匿名 typedef，必须拿到完整定义 */
/* cx/cy 为设备像素中心，d 为直径（设备像素），k 为整体淡入系数 0..1。
   走 canvas_blend_px 按 alpha 合成，可在圆角卡片上直接叠加。 */
void capsule_icon_draw(Canvas *c, double cx, double cy, double d, double k);
#endif
