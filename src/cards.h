/* cards.h — 卡片注册表与导航 */
#ifndef QISLAND_CARDS_H
#define QISLAND_CARDS_H

#include "common.h"

/* 尺寸档 */
typedef enum { TIER_MID = 0, TIER_BIG = 1, TIER_HUGE = 2 } CardTier;

typedef enum {
    DRAW_TIMER = 0,
    DRAW_NOTE  = 1,
    DRAW_PERF  = 2,
    DRAW_EMPTY = 3,
    DRAW_CLIP  = 4,
    DRAW_DATE  = 5,
    DRAW_SETTINGS = 6
} CardDraw;

typedef enum { GRP_V = 0, GRP_H = 1, GRP_COUNT = 2 } CardGroup;

typedef struct {
    const char *id;
    CardTier    tier;
    CardDraw    draw;
    CardGroup   group;
    int         slot;
    int         vdot;
    int         isDefault;
} CardDef;

#define CARDS_VDOTS     5        /* 竖向指示点 */
#define CARDS_MAX       64       /* 表容量上限 */

int        cards_count(void);
const CardDef *cards_at(int idx);
CardTier   cards_tier(int idx);
CardDraw   cards_draw(int idx);
CardGroup  cards_group(int idx);
int        cards_vdot(int idx);
int        cards_slot(int idx);

int        cards_hmove(int idx, int dir);
int        cards_group_count(CardGroup g);
int        cards_group_card(CardGroup g, int slot);

int        cards_is_hgroup(CardGroup g);
int        cards_hgroup_count(void);
int        cards_group_default(CardGroup g);
int        cards_group_first(CardGroup g);
int        cards_group_last(CardGroup g);
CardGroup  cards_group_by_order(int k);


int        cards_next(int idx, int dir);
int        cards_anchor(int vdot);
int        cards_last_card(void);

#endif /* QISLAND_CARDS_H */
