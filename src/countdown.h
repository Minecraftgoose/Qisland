/* countdown.h — 倒计时状态机 */
#ifndef QISLAND_COUNTDOWN_H
#define QISLAND_COUNTDOWN_H

#include "common.h"

typedef struct {
    int    running;
    int    ended;
    DWORD  start;        /* 本段计时开始时刻 */
    double base;         /* 本段起始秒数 */
    double remain;       /* 暂停时剩余秒数 */
} Countdown;

void   countdown_init(Countdown *c);
void   countdown_reset(Countdown *c);
void   countdown_toggle(Countdown *c);
double countdown_remain(const Countdown *c);
void   countdown_stop_if_zero(Countdown *c);
int    countdown_adjust(Countdown *c, int seg, int step);
void   countdown_clear_ended(Countdown *c);

#endif /* QISLAND_COUNTDOWN_H */
