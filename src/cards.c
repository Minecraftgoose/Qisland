/* cards.c — 卡片注册表（唯一一处定义「有哪些卡片」）
 *
 * 定稿：**6 张卡、5 个竖点、一个竖向区、唯一一条横向（3 个点）**。
 *   idx 0  timer   MID  V / vdot0              竖①
 *   idx 1  date    MID  V / vdot1              竖②
 *                  ← 本轮新增：带秒时钟 + 农历/干支/节气/周数等一整组日期
 *   idx 2  note    MID  V / vdot2              竖③
 *   idx 3  perf    MID  H / slot0 / vdot3      竖④，同时是横向第 1 位
 *                （GRP_H 但 vdot=2，只在竖向阶段露面 → 显示竖列点）
 *   idx 4  big1    BIG  H / slot1 / vdot4 / default / DRAW_CLIP
 *                竖⑤，同时是横向第 2 位
 *                第一张大卡片 = 剪贴板历史记录
 *   idx 5  huge1   HUGE H / slot2 / vdot4 / DRAW_SETTINGS
 *                横向第 3 位 = **最后一点**，唯一的超大卡片 = 设置页
 *
 * 用户把前两版的「只留大卡」/「只留超大卡」合并成「两个都要」，
 * 但「只有一个横向」的约束保留：两张卡同在 GRP_H，靠 slot 排在同一排
 * 横点里，不拆成第二个横向组。
 * 已删除：big2/3/4、huge0/2、整个 GRP_U。
 * 超大卡片先按用户要求留空（DRAW_EMPTY）过一版，本轮改成设置页：
 * 左右栏「通用 / 关于」，通用页三个收起条件 + 开机自启动；关于页留空。
 * 底部横排圆点（全局导航指示器）仍照常绘制，不进 cards[] 内容。
 *
 * 以后要加卡片：在下面 cards[] 里插一行，填好 tier / draw / group /
 * slot / vdot 即可。导航、竖点、尺寸、诊断表全部由这张表推导，
 * 不需要改 ui.c / anim.c / window.c / tests.c 里任何数字。
 */
#include "cards.h"
#include "config.h"

static const CardDef cards[] = {
    /* idx  id       tier        draw        group   slot  vdot  default */
    { "timer",  TIER_MID,   DRAW_TIMER, GRP_V,   0,    0,    0 },
    { "date",   TIER_MID,   DRAW_DATE,  GRP_V,   0,    1,    0 },  /* 第二张中卡片 */
    { "note",   TIER_MID,   DRAW_NOTE,  GRP_V,   0,    2,    0 },
    { "perf",   TIER_MID,   DRAW_PERF,  GRP_H,   0,    3,    0 },
    { "big1",   TIER_BIG,   DRAW_CLIP,  GRP_H,   1,    4,    1 },  /* 竖⑤ = 横2，剪贴板历史 */
    { "huge1",  TIER_HUGE,  DRAW_SETTINGS, GRP_H, 2,    4,    0 }, /* 横3 = 最后一点，设置页 */
};

static int card_n(void)
{
    int n = (int)(sizeof(cards) / sizeof(cards[0]));
    return n < CARDS_MAX ? n : CARDS_MAX;
}

int cards_count(void) { return card_n(); }

const CardDef *cards_at(int idx)
{
    int n = card_n();
    if (idx < 0) idx = 0;
    if (idx >= n) idx = n - 1;
    return &cards[idx];
}

CardTier  cards_tier(int idx)  { return cards_at(idx)->tier; }
CardDraw  cards_draw(int idx)  { return cards_at(idx)->draw; }
CardGroup cards_group(int idx) { return cards_at(idx)->group; }
int       cards_vdot(int idx)  { return cards_at(idx)->vdot; }
int       cards_slot(int idx)  { return cards_at(idx)->slot; }

/* 组内找指定 slot 的卡片；找不到返回 -1 */
static int find_slot(CardGroup g, int slot)
{
    int n = card_n(), i;
    for (i = 0; i < n; i++)
        if (cards[i].group == g && cards[i].slot == slot) return i;
    return -1;
}

/* 组内的 default 卡；没有则返回 -1 */
static int find_default(CardGroup g)
{
    int n = card_n(), i;
    for (i = 0; i < n; i++)
        if (cards[i].group == g && cards[i].isDefault) return i;
    return -1;
}

int cards_hmove(int idx, int dir)
{
    const CardDef *c = cards_at(idx);
    int j = find_slot(c->group, c->slot + (dir > 0 ? 1 : -1));
    return (j >= 0) ? j : qs_iclamp(idx, 0, card_n() - 1);
}

int cards_group_count(CardGroup g)
{
    int n = card_n(), i, cnt = 0;
    for (i = 0; i < n; i++)
        if (cards[i].group == g) cnt++;
    return cnt;
}

int cards_group_card(CardGroup g, int slot)
{
    return find_slot(g, slot);
}

/* ---- 横向组通用查询（表驱动，不写死组数）---- */
int cards_is_hgroup(CardGroup g)
{
    return (g != GRP_V && (int)g > 0 && (int)g < GRP_COUNT) ? 1 : 0;
}

int cards_hgroup_count(void)
{
    int n = card_n(), i, g, cnt = 0;
    int seen[GRP_COUNT];
    for (g = 0; g < GRP_COUNT; g++) seen[g] = 0;
    for (i = 0; i < n; i++) {
        g = (int)cards[i].group;
        if (g < 0 || g >= GRP_COUNT || !cards_is_hgroup((CardGroup)g)) continue;
        if (!seen[g]) { seen[g] = 1; cnt++; }
    }
    return cnt;
}

int cards_group_default(CardGroup g) { return find_default(g); }

int cards_group_first(CardGroup g)
{
    int n = card_n(), i, best = -1, bestSlot = 0x7fffffff;
    for (i = 0; i < n; i++)
        if (cards[i].group == g && cards[i].slot < bestSlot) {
            bestSlot = cards[i].slot; best = i;
        }
    return best;
}

int cards_group_last(CardGroup g)
{
    int n = card_n(), i, best = -1, bestSlot = -1;
    for (i = 0; i < n; i++)
        if (cards[i].group == g && cards[i].slot > bestSlot) {
            bestSlot = cards[i].slot; best = i;
        }
    return best;
}

CardGroup cards_group_by_order(int k)
{
    int n = card_n(), i, g, cnt = 0;
    int seen[GRP_COUNT];
    for (g = 0; g < GRP_COUNT; g++) seen[g] = 0;
    for (i = 0; i < n; i++) {
        g = (int)cards[i].group;
        if (g < 0 || g >= GRP_COUNT || seen[g]) continue;
        seen[g] = 1;
        if (cnt == k) return (CardGroup)g;
        cnt++;
    }
    return GRP_V;
}

int cards_next(int idx, int dir)
{
    int n = card_n();
    int i = qs_iclamp(idx, 0, n - 1);
    int j = i + (dir > 0 ? 1 : -1);
    CardGroup gi, gj;
    int d;

    if (j < 0 || j >= n) return i;             /* 整条序列到头停住 */
    gi = cards[i].group;
    gj = cards[j].group;
    if (gj == gi) return j;                    /* 同组：线性走 */

    /* 只有「横向组 ↔ 横向组」的跨组才落目标组 default。
       当前只有一个横向组，这段不会触发；V→H 保持线性邻接
       （0→1→2→3→4 能一路滚到性能页、大卡片与超大卡片），不会被 default 跳掉。
       保留这段是为了以后真拆成多组时不用再改这里。 */
    if (cards_is_hgroup(gi) && cards_is_hgroup(gj)) {
        d = cards_group_default(gj);
        if (d >= 0) return d;
    }
    return j;
}

int cards_anchor(int vdot)
{
    int n = card_n(), i;
    for (i = 0; i < n; i++)
        if (cards[i].vdot == vdot) return i;
    return 0;
}

int cards_last_card(void)
{
    int j = cards_group_last(GRP_H);
    return (j >= 0) ? j : card_n() - 1;
}
