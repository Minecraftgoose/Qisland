/* countdown.c — 用墙钟推算剩余时间，不累加定时器误差 */
#include "countdown.h"
#include "config.h"

void countdown_init(Countdown *c)
{
    ZeroMemory(c, sizeof(*c));
    c->remain = (double)CD_DEFAULT_SEC;
    c->base   = c->remain;
}

void countdown_reset(Countdown *c)
{
    c->running = 0;
    c->ended   = 0;
    c->remain  = (double)CD_DEFAULT_SEC;
    c->base    = c->remain;
}

double countdown_remain(const Countdown *c)
{
    if (!c->running) return c->remain;
    {
        double e = (double)(qs_now() - c->start) / 1000.0;
        double r = c->base - e;
        return r > 0.0 ? r : 0.0;
    }
}

void countdown_toggle(Countdown *c)
{
    c->ended = 0;
    if (c->running) {
        c->remain = countdown_remain(c);
        c->running = 0;
    } else {
        c->base = c->remain > 0.5 ? c->remain : (double)CD_DEFAULT_SEC;
        c->start = qs_now();
        c->running = 1;
    }
}

void countdown_stop_if_zero(Countdown *c)
{
    if (c->running && countdown_remain(c) <= 0.0) {
        c->running = 0;
        c->remain  = 0.0;
        c->ended   = 1;      /* 自然归零 → 触发自动展开提示 */
    }
}

void countdown_clear_ended(Countdown *c)
{
    c->ended = 0;
}

int countdown_adjust(Countdown *c, int seg, int step)
{
    double r = countdown_remain(c);
    double unit = (seg == 0) ? (double)CD_STEP_H
                : (seg == 1) ? (double)CD_STEP_M
                             : (double)CD_STEP_S;
    double nr = r + (double)step * unit;
    if (nr < 0.0) nr = 0.0;
    if (nr > (double)CD_MAX_SEC) nr = (double)CD_MAX_SEC;
    if (fabs(nr - r) < 0.001) return 0;

    c->remain = nr;
    c->base   = nr;
    if (c->running) c->start = qs_now();   /* 运行中：从新剩余值重新起一段 */
    c->ended  = 0;
    return 1;
}
