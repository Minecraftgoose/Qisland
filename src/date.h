/* date.h — 带秒的时钟 历法日期 */
#ifndef QISLAND_DATE_H
#define QISLAND_DATE_H

#include "common.h"
#include "font.h"
#include "canvas.h"

void date_draw(FontSet *f, Canvas *c, double W, double H, double k);

#endif /* QISLAND_DATE_H */
