/* about.h — 设置页「关于」右栏*/
#ifndef QISLAND_ABOUT_H
#define QISLAND_ABOUT_H

#include "common.h"
#include "settings.h"

void about_draw(App *a, double W, double H, double L,
                const SetBoxes *b, double k);

void about_wheel(App *a, double viewH, int dir);

#endif /* QISLAND_ABOUT_H */
