/* cards.h — 卡片注册表与导航（表驱动，可持续加卡片）
 *
 * 用户最终定稿：**大卡片要，超大卡片也要，但仍然只有一个横向组**。
 *
 *   竖①  计时器（中）                       GRP_V
 *   竖②  便签（中）                         GRP_V
 *   竖③  性能监测（中）← 同时是横向第 1 位   GRP_H slot 0（竖向阶段露面）
 *   竖④  大卡片     ← 同时是横向第 2 位     GRP_H slot 1 / default
 *
 *   ── 唯一一条横向（3 点）：性能 → 大卡片 → 超大卡片 ──
 *                                      横1      横2        横3 = 最后一点
 *
 * 同时满足这几轮的所有约束：
 *   · 「大卡片我要，超大也要」             → 两个尺寸档各一张，都在表里
 *   · 「只有一个横向」                     → 仍只有 GRP_H 一个横向组
 *   · 「横向最后一个点是超大卡片」         → slot2 / TIER_HUGE
 *   · 「竖向第四个就是横向第二个」         → vdot3 锚点 = slot1 的大卡片
 *   · 4 个竖点、往下滚 = 下一个卡片       → 不变
 *
 * 已删除：big2/3/4（多加的大卡）、huge0/2（多加的超大卡）、第二个横向组 GRP_U、
 * 所有留空卡。全表 5 张卡。
 *
 * 每张卡带 tier（尺寸档）/ draw（渲染器）/ group（V 竖向 / H 横向）
 * / slot（组内序号）/ vdot（对应哪个竖点）/ isDefault（跨组进入时的落点）。
 * 加卡片 = 往 cards[] 里插一行，点数、尺寸、滚轮序列、诊断表全部自动推导。
 */
#ifndef QISLAND_CARDS_H
#define QISLAND_CARDS_H

#include "common.h"

/* 尺寸档：具体像素仍在 config.h / anim.c 里。
   当前 cards[] 里 TIER_BIG 一张、TIER_HUGE 一张。 */
typedef enum { TIER_MID = 0, TIER_BIG = 1, TIER_HUGE = 2 } CardTier;

/* 渲染器：EMPTY = 只有卡片形状，内容先留空；
   CLIP = 粘贴板历史（第一个大卡片） */
typedef enum {
    DRAW_TIMER = 0,
    DRAW_NOTE  = 1,
    DRAW_PERF  = 2,
    DRAW_EMPTY = 3,
    DRAW_CLIP  = 4
} CardDraw;

/* 组：V=竖向导航区；H=**唯一**的横向组。
   枚举值即组顺序，GRP_COUNT 供表驱动查询遍历用。 */
typedef enum { GRP_V = 0, GRP_H = 1, GRP_COUNT = 2 } CardGroup;

typedef struct {
    const char *id;      /* 诊断用短名 */
    CardTier    tier;
    CardDraw    draw;
    CardGroup   group;
    int         slot;    /* 组内序号，从 0 开始 */
    int         vdot;    /* 对应竖点 0..CARDS_VDOTS-1 */
    int         isDefault;  /* 跨组进入时的落点 */
} CardDef;

#define CARDS_VDOTS     4        /* 竖向指示点：5 → 4 */
#define CARDS_MAX       64       /* 表容量上限；以后加卡片别超这个，超了改一行 */

int        cards_count(void);
const CardDef *cards_at(int idx);
CardTier   cards_tier(int idx);
CardDraw   cards_draw(int idx);
CardGroup  cards_group(int idx);
int        cards_vdot(int idx);
int        cards_slot(int idx);

/* 组内横向移动：返回同组内 slot ±1 的卡片索引，越界返回原值 */
int        cards_hmove(int idx, int dir);
/* 组内有几张卡 / 该组第 slot 张的卡片索引（找不到返回 -1）。
   横排点数、点击第几个点跳哪张卡全靠这两个。 */
int        cards_group_count(CardGroup g);
int        cards_group_card(CardGroup g, int slot);

/* ---- 横向组通用查询（表驱动，不写死组数）---- */
int        cards_is_hgroup(CardGroup g);       /* GRP_H → 1，GRP_V → 0 */
int        cards_hgroup_count(void);           /* 表里实际出现的横向组数 = 1 */
int        cards_group_default(CardGroup g);   /* 该组 isDefault 卡索引，无则 -1 */
int        cards_group_first(CardGroup g);
int        cards_group_last(CardGroup g);      /* 横向最后一点 = 超大卡片 */
CardGroup  cards_group_by_order(int k);

/* 竖向主滚轮：dir=+1 往下滚（下一个），-1 往上滚。
   V→H 保持线性邻接（能正常滚到性能页）；横向组之间若以后拆成多组，
   跨组才落到目标组 default 项 —— 当前只有一组，永远线性走。 */
int        cards_next(int idx, int dir);
/* 某个竖点对应的锚点卡片（右键菜单 4 个点用） */
int        cards_anchor(int vdot);
/* 横向最后一个点（= 超大卡片）；诊断 / 测试 / 导出图用 */
int        cards_last_card(void);

#endif /* QISLAND_CARDS_H */
