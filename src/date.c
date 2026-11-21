/* date.c — 第二张中卡片：带秒的时钟 + 一整组历法日期
 *
 * 纯 Win32 / 纯 C，无运行时分配、无状态、无定时器、无文件、无网络。
 *   · 时间源：GetLocalTime（Win7 原生，跟随用户时区）
 *   · 农历 / 闰月 / 干支 / 生肖 / 节日 / 24 节气 / 每日宜忌：
 *     全部来自 src/vendor/ 的两套离线静态表，经 src/lunar.c 同步封装。
 *   · ISO 周数、年内第几天 / 剩余天数：本地 days_from_civil 计算
 *   · 文字全部走 font_draw_label（全局 OPPO Sans、灰度抗锯齿），
 *     数字时钟逐字形绘制，字形缓存长期稳定，秒变化不会每帧冲掉缓存。
 *
 * 版式按用户给的 HTML「紧凑型长条屏幕排版」：
 *   grid 1.2fr | 1px | 1fr，两侧 padding 24px，gap 20px；
 *   左块 .time / .date-row / .almanac-row，右块 .lunar-year / .lunar-date /
 *   .ganzhi / .extra-info；两块各自 flex-column + justify-content:center，
 *   即**整组垂直居中**，不再把每一行钉死在 H 的固定比例上。
 *   只有 .label.yi / .label.ji 带色，事项与其它信息都是中性灰白。
 * 排版基准用目标态标称尺寸 W0/H0，动画期间文字只平移淡入、不重排。
 */
#include "date.h"
#include "config.h"
#include "lunar.h"

/* ---------------- 公历 ↔ 天数（Hinnant civil 算法，避开 time_t / 时区） ---------------- */
static long days_from_civil(int y, int m, int d)
{
    long era, doe;
    unsigned yoe, doy;
    y -= (m <= 2);
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = (unsigned)(y - era * 400);
    doy = (unsigned)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
    doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return (long)(era * 146097 + (long)doe - 719468);
}

static int is_leap(int y)
{
    return ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0) ? 1 : 0;
}

static int iso_weekday_from_days(long z)
{
    /* 1970-01-01 = 周四(4)；ISO：1=周一 .. 7=周日 */
    long w = ((z % 7) + 7) % 7;
    return (int)(((w + 3) % 7) + 1);
}

static int iso_weeks_in_year(int y)
{
    int w = iso_weekday_from_days(days_from_civil(y, 1, 1));
    return (w == 4 || (is_leap(y) && w == 3)) ? 53 : 52;
}

static int iso_week(int y, int m, int d)
{
    int doy = (int)(days_from_civil(y, m, d) - days_from_civil(y, 1, 1)) + 1;
    int wd  = iso_weekday_from_days(days_from_civil(y, m, d));
    int wk  = (doy - wd + 10) / 7;
    if (wk < 1) return iso_weeks_in_year(y - 1);
    if (wk > iso_weeks_in_year(y)) return 1;
    return wk;
}

static const WCHAR *kWeek[7] = {
    L"日",L"一",L"二",L"三",L"四",L"五",L"六"
};

/* ---------------- 时钟：逐字形绘制，字形缓存不随秒变化抖动 ---------------- */
static void draw_clock(FontSet *f, Canvas *c, int hh, int mm, int ss,
                       int px, double x, double y, double k)
{
    WCHAR digs[8];
    double step  = (double)px * DATE_DIGIT_STEP;
    double colon = (double)px * DATE_COLON_W;
    double rad   = qs_clamp((double)px * 0.046, 1.4, 3.2);
    double dy    = (double)px * 0.145;
    double cx = x;
    int i;

    _snwprintf(digs, 8, L"%02d%02d%02d", hh, mm, ss);
    for (i = 0; i < 6; i++) {
        if (i == 2 || i == 4) {
            double ccx = cx + colon * 0.5;
            canvas_disc(c, ccx, y - dy, rad, INK_R, INK_G, INK_B, k * 0.82);
            canvas_disc(c, ccx, y + dy, rad, INK_R, INK_G, INK_B, k * 0.82);
            cx += colon;
        }
        {
            WCHAR ch[2];
            int r = INK_R, g = INK_G, b = INK_B;
            double kk = k;
            ch[0] = digs[i]; ch[1] = 0;
            if (i >= 4) {                 /* 秒：略淡一档，层次不抢时分 */
                r = ACCENT_R; g = ACCENT_G; b = ACCENT_B; kk = k * 0.88;
            }
            font_draw_text(f, c, ch, px, cx + step * 0.5, y, 1, r, g, b, kk);
        }
        cx += step;
    }
}

/* ---------------- 整组垂直居中 ----------------
   HTML 里 .left-block / .right-block 都是 justify-content:center，
   两栏各自作为一个整体居中，而不是每一行钉死在 H 的固定比例上。
   传入每行高度与「该行之后的间距」，算出总高后居中，返回每行中心 y。 */
static void layout_rows(const double *h, const double *gapAfter,
                        int n, double H, double *outY)
{
    double total = 0.0, y;
    int i;
    for (i = 0; i < n; i++) total += h[i] + gapAfter[i];
    total -= gapAfter[n - 1];
    y = (H - total) * 0.5;
    for (i = 0; i < n; i++) {
        outY[i] = y + h[i] * 0.5;
        y += h[i] + gapAfter[i];
    }
}

/* ---------------- 节日：并进日期行，不新开一行 ----------------
   HTML 的 .date-row 只有「公历 + 星期·第N周」。节日仍保留（用户要过
   「一大堆日期」），但降级成同一行末尾的附属：左对齐、按剩余宽度逐级
   缩字号，实在放不下就逐字截断，绝不换行、绝不压到分隔线。 */
static void draw_festival(FontSet *f, Canvas *c, const WCHAR *s,
                          double x, double maxW, double y,
                          int basePx, int minPx, double k)
{
    WCHAR tmp[32];
    int len, px;

    if (maxW <= 0) return;
    _snwprintf(tmp, 32, L"%s", s);
    tmp[31] = 0;
    len = (int)wcslen(tmp);
    if (len <= 0) return;

    for (px = basePx; px > minPx; px--)
        if (font_measure(f, tmp, len, px, 2) <= maxW) break;
    while (len > 1 && font_measure(f, tmp, len, px, 2) > maxW)
        tmp[--len] = 0;
    if (font_measure(f, tmp, len, px, 2) <= maxW)
        font_draw_label(f, c, tmp, px, x, y, 0,
                        ACCENT_R, ACCENT_G, ACCENT_B, k * 0.95);
}

/* ---------------- 宜 / 忌：只有标签带色，事项中性 ----------------
   照 HTML：.label.yi 绿、.label.ji 红，.almanac-text 一律 #ced4da。
   上一版整行带色，两行彩色块压在底部显得乱；改回只有两个标签有色，
   事项与右栏辅助信息同档次灰白，整页就安静了。标签与事项同字号。 */
static void draw_yiji(FontSet *f, Canvas *c, const WCHAR *tag,
                      WCHAR items[3][16], int n,
                      double x, double y, double maxW, int basePx,
                      int r, int g, int b, double k)
{
    WCHAR buf[96];
    int px, len, w;
    double tagW, gap;

    if (n > 3) n = 3;
    if (n < 1) n = 1;
    _snwprintf(buf, 96, L"%s%s%s%s%s%s", items[0],
               n > 1 ? L" " : L"", n > 1 ? items[1] : L"",
               n > 2 ? L" " : L"", n > 2 ? items[2] : L"", L"");
    buf[95] = 0;
    len = (int)wcslen(buf);
    if (len <= 0) return;

    for (px = basePx; px > 11; px--) {
        tagW = (double)font_measure(f, tag, 1, px, 2);
        gap = 8.0;                         /* HTML .label margin-right: 8px */
        w = font_measure(f, buf, len, px, 2);
        if (tagW + gap + (double)w <= maxW) break;
    }
    tagW = (double)font_measure(f, tag, 1, px, 2);
    gap = 8.0;

    while (len > 1) {
        w = font_measure(f, buf, len, px, 2);
        if (tagW + gap + (double)w <= maxW) break;
        buf[--len] = 0;
    }
    font_draw_label(f, c, tag, px, x, y, 0, r, g, b, k);
    font_draw_label(f, c, buf, px, x + tagW + gap, y, 0,
                    DATE_YIJI_TEXT_R, DATE_YIJI_TEXT_G, DATE_YIJI_TEXT_B,
                    k * 0.92);
}

/* ---------------- 主绘制 ---------------- */
void date_draw(FontSet *f, Canvas *c, double W, double H, double k)
{
    SYSTEMTIME st;
    double L, leftX, rightX, sepX;
    double h[5], g[5], y[5];
    double yClock, yDate, yYi, yJi;
    double yYear, yLunar, yGZ, yTerm, yCount;
    double gregW, weekW, festX, yijiW;
    int clockPx, gregPx, smallPx, weekPx;
    int lunarPx, lunarBigPx, termPx, festivalPx, yijiPx;
    int doy, week, ydays;
    WCHAR buf[96], weekBuf[64];
    LunarInfo li;
    int live;

    GetLocalTime(&st);
    L = ((double)c->W - W) * 0.5;

    leftX  = L + W * DATE_LEFT_X;
    sepX   = L + W * DATE_SEP_X;
    rightX = L + W * DATE_RIGHT_X;

    clockPx    = qs_iclamp((int)(H * DATE_CLOCK_PX     + 0.5), 22, 52);
    gregPx     = qs_iclamp((int)(H * DATE_GREG_PX      + 0.5), 12, 26);
    smallPx    = qs_iclamp((int)(H * DATE_SMALL_PX     + 0.5), 11, 20);
    weekPx     = smallPx - 3; if (weekPx < 10) weekPx = 10;
    lunarPx    = qs_iclamp((int)(H * DATE_LUNAR_PX      + 0.5), 12, 24);
    lunarBigPx = qs_iclamp((int)(H * DATE_LUNAR_BIG_PX  + 0.5), 14, 28);
    termPx     = qs_iclamp((int)(H * DATE_TERM_PX       + 0.5), 13, 26);
    festivalPx = qs_iclamp((int)(H * DATE_FESTIVAL_PX   + 0.5), 14, 28);
    yijiPx     = qs_iclamp((int)(H * DATE_YIJI_PX       + 0.5), 12, 22);

    live = lunar_get(st.wYear, st.wMonth, st.wDay, &li);

    /* 左栏 4 行：时钟 / 日期行 / 宜 / 忌 —— 整组居中 */
    h[0] = (double)clockPx * DATE_ROW_CLOCK;  g[0] = H * DATE_GAP_A;
    h[1] = (double)gregPx  * DATE_ROW_TEXT;   g[1] = H * DATE_GAP_B;
    h[2] = (double)yijiPx  * DATE_ROW_TEXT;   g[2] = H * DATE_GAP_YIJI;
    h[3] = (double)yijiPx  * DATE_ROW_TEXT;   g[3] = 0.0;
    layout_rows(h, g, 4, H, y);
    yClock = y[0]; yDate = y[1]; yYi = y[2]; yJi = y[3];

    /* 右栏 5 行：干支年 / 农历 / 干支月日 / 节气 / 年内天数 —— 整组居中 */
    h[0] = (double)lunarPx    * DATE_ROW_TEXT;  g[0] = H * DATE_GAP_RIGHT;
    h[1] = (double)lunarBigPx * DATE_ROW_TEXT;  g[1] = H * DATE_GAP_RIGHT;
    h[2] = (double)smallPx    * DATE_ROW_TEXT;  g[2] = H * DATE_GAP_RIGHT;
    h[3] = (double)termPx     * DATE_ROW_TEXT;  g[3] = H * DATE_GAP_RIGHT;
    h[4] = (double)smallPx    * DATE_ROW_TEXT;  g[4] = 0.0;
    layout_rows(h, g, 5, H, y);
    yYear = y[0]; yLunar = y[1]; yGZ = y[2]; yTerm = y[3]; yCount = y[4];

    /* .divider：1px 极淡竖线，近全高；不做装饰框 */
    canvas_round(c, sepX, H * 0.075, 1.2, H * 0.850, 0.6,
                 TRACK_R, TRACK_G, TRACK_B, k * 0.85);

    /* ================= 左栏 ================= */
    draw_clock(f, c, st.wHour, st.wMinute, st.wSecond,
               clockPx, leftX, yClock, k);

    /* .date-row：公历 + 星期·第N周（+ 可选节日）挤在同一行 */
    _snwprintf(buf, 96, L"%d年%d月%d日", st.wYear, st.wMonth, st.wDay);
    font_draw_label(f, c, buf, gregPx, leftX, yDate, 0,
                    ACCENT_R, ACCENT_G, ACCENT_B, k * 0.95);
    gregW = (double)font_measure(f, buf, (int)wcslen(buf), gregPx, 2);

    week = iso_week(st.wYear, st.wMonth, st.wDay);
    _snwprintf(weekBuf, 64, L"星期%s · 第%d周", kWeek[st.wDayOfWeek], week);
    weekW = (double)font_measure(f, weekBuf, (int)wcslen(weekBuf), weekPx, 2);
    font_draw_label(f, c, weekBuf, weekPx,
                    leftX + gregW + DATE_GREG_GAP, yDate, 0,
                    DIM_R, DIM_G, DIM_B, k * 0.92);

    if (live && li.festival[0]) {
        festX = leftX + gregW + DATE_GREG_GAP + weekW + DATE_GREG_GAP;
        draw_festival(f, c, li.festival, festX,
                      sepX - W * 0.020 - festX, yDate,
                      festivalPx, weekPx, k);
    }

    /* .almanac-row：宜 / 忌 两行，标签有色、事项中性 */
    if (live) {
        yijiW = sepX - leftX - W * 0.030;
        draw_yiji(f, c, L"宜", li.yi, li.yiN, leftX, yYi, yijiW, yijiPx,
                  DATE_YI_R, DATE_YI_G, DATE_YI_B, k);
        draw_yiji(f, c, L"忌", li.ji, li.jiN, leftX, yJi, yijiW, yijiPx,
                  DATE_JI_R, DATE_JI_G, DATE_JI_B, k);
    }

    /* ================= 右栏 ================= */
    if (live) {
        /* .lunar-year：最淡一档 */
        _snwprintf(buf, 96, L"%s年 · %s年", li.yearGZ, li.animal);
        font_draw_label(f, c, buf, lunarPx - 1, rightX, yYear, 0,
                        DIM_R, DIM_G, DIM_B, k * 0.88);

        /* .lunar-date：本栏最亮的主信息，20px 没退回 18px */
        _snwprintf(buf, 96, L"农历%s%s", li.monthCn, li.dayCn);
        font_draw_label(f, c, buf, lunarBigPx, rightX, yLunar, 0,
                        INK_R, INK_G, INK_B, k);

        _snwprintf(buf, 96, L"%s月 · %s日", li.monthGZ, li.dayGZ);
        font_draw_label(f, c, buf, smallPx, rightX, yGZ, 0,
                        DIM_R, DIM_G, DIM_B, k * 0.92);

        /* 日历同款口径：节气名只写在交节那一格；11/18 是「距小雪4天」。 */
        if (li.nextTerm[0]) {
            if (li.termToday && li.curTerm[0])
                _snwprintf(buf, 96, L"%s · 距%s%d天",
                           li.curTerm, li.nextTerm, li.daysToNextTerm);
            else
                _snwprintf(buf, 96, L"距%s%d天",
                           li.nextTerm, li.daysToNextTerm);
            font_draw_label(f, c, buf, termPx, rightX, yTerm, 0,
                            DIM_R, DIM_G, DIM_B, k * 0.92);
        } else if (li.curTerm[0]) {
            font_draw_label(f, c, li.curTerm, termPx, rightX, yTerm, 0,
                            DIM_R, DIM_G, DIM_B, k * 0.92);
        }
    }

    doy = (int)(days_from_civil(st.wYear, st.wMonth, st.wDay) -
                days_from_civil(st.wYear, 1, 1)) + 1;
    ydays = is_leap(st.wYear) ? 366 : 365;
    _snwprintf(buf, 96, L"第%d天 · 余%d天", doy, ydays - doy);
    font_draw_label(f, c, buf, smallPx, rightX, yCount, 0,
                    DIM_R, DIM_G, DIM_B, k * 0.82);
}
