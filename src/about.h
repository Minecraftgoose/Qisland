/* about.h — 设置页「关于」右栏（软件图标 + 名称 + 仓库链接 + 四个胶囊板块） */
#ifndef QISLAND_ABOUT_H
#define QISLAND_ABOUT_H

#include "common.h"
#include "settings.h"

/* 由 settings_draw() 在 tab == SET_TAB_ABOUT 时调用。 W/H/L 是目标态标称尺寸（设备像素），k 是展开淡入系数。 */
void about_draw(App *a, double W, double H, double L,
                const SetBoxes *b, double k);

/* dir > 0 = 往下滚（内容上移）。 viewH 取 aboutBody.h。 */
void about_wheel(App *a, double viewH, int dir);

#endif /* QISLAND_ABOUT_H */
