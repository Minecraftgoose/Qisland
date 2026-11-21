/* lunar.c — 本地农历库封装实现 */
#include "lunar.h"
#include "vendor/lunar_date.h"
#include "vendor/solar_term.h"
#include "vendor/huangli_data.h"

/* 库返回 UTF-8； 项目内部一律 WCHAR。 */
static void u8_to_w(const char *s, WCHAR *out, int cap)
{
    int n;
    if (!s || !*s || cap <= 0) { if (cap > 0) out[0] = 0; return; }
    n = MultiByteToWideChar(CP_UTF8, 0, s, -1, out, cap - 1);
    if (n <= 0) out[0] = 0;
    else out[n] = 0;
}

/* 与 date.c 同款的 Hinnant civil 天数。 */
static long daynum(int y, int m, int d)
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

/* 60 甲子 */
static const WCHAR *const kJiaZi[60] = {
    L"甲子", L"乙丑", L"丙寅", L"丁卯", L"戊辰", L"己巳",
    L"庚午", L"辛未", L"壬申", L"癸酉", L"甲戌", L"乙亥",
    L"丙子", L"丁丑", L"戊寅", L"己卯", L"庚辰", L"辛巳",
    L"壬午", L"癸未", L"甲申", L"乙酉", L"丙戌", L"丁亥",
    L"戊子", L"己丑", L"庚寅", L"辛卯", L"壬辰", L"癸巳",
    L"甲午", L"乙未", L"丙申", L"丁酉", L"戊戌", L"己亥",
    L"庚子", L"辛丑", L"壬寅", L"癸卯", L"甲辰", L"乙巳",
    L"丙午", L"丁未", L"戊申", L"己酉", L"庚戌", L"辛亥",
    L"壬子", L"癸丑", L"甲寅", L"乙卯", L"丙辰", L"丁巳",
    L"戊午", L"己未", L"庚申", L"辛酉", L"壬戌", L"癸亥"
};
static const WCHAR *kGan = L"甲乙丙丁戊己庚辛壬癸";
static const WCHAR *kZhi = L"子丑寅卯辰巳午未申酉戌亥";

static int jiazi_index(const WCHAR *gz)
{
    int i;
    for (i = 0; i < 60; i++)
        if (wcscmp(gz, kJiaZi[i]) == 0) return i;
    return -1;
}

static long term_daynum(int y, int n)
{
    LUNAR_DATE td;
    if (!get_day_of_solar_term((uint32_t)y, (uint8_t)n, &td)) return 0;
    return daynum(td.year, td.month, td.day);
}

static void month_pillar_by_term(int y, int m, int d, WCHAR *out, int cap)
{
    /* 必须按一年内的**时间先后**排列：小寒(1月) → 立春 → ... → 大雪(12月)。 */
    static const struct { int term; int k; } kJie[12] = {
        { 0, 11 }, { 2, 0 }, { 4, 1 }, { 6, 2 }, { 8, 3 }, { 10, 4 },
        {12,  5 }, {14, 6 }, {16, 7 }, {18, 8 }, {20, 9 }, { 22, 10 }
    };
    long today = daynum(y, m, d);
    long liChun = term_daynum(y, 2);
    int k, i, ganYear, gan, zhi;
    WCHAR gz[3];

    /* 默认子月：1 月 1 日~小寒前仍在上一年大雪之后 */
    k = 10;
    for (i = 0; i < 12; i++) {
        long t = term_daynum(y, kJie[i].term);
        if (t && t <= today) k = kJie[i].k;
    }

    ganYear = (today >= liChun) ? y : y - 1;
    gan = (ganYear - 4) % 10; if (gan < 0) gan += 10;
    zhi = (2 + k) % 12;
    gan = (gan * 2 + 2 + k) % 10;           /* 五虎遁 */
    gz[0] = kGan[gan]; gz[1] = kZhi[zhi]; gz[2] = 0;
    _snwprintf(out, cap, L"%s", gz);
    out[cap - 1] = 0;
}

static void hex2(int n, char out[3])
{
    static const char H[] = "0123456789ABCDEF";
    out[0] = H[(n >> 4) & 15]; out[1] = H[n & 15]; out[2] = 0;
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

static void take_words(const char *p, int len, WCHAR out[3][16], int *cnt)
{
    int i = 0, n = 0;
    *cnt = 0;
    for (i = 0; i + 1 < len && n < 3; i += 2) {
        int idx = hexval(p[i]) * 16 + hexval(p[i + 1]);
        if (idx < 0 || idx >= HL_WORD_COUNT) continue;
        _snwprintf(out[n], 16, L"%s", HL_WORDS[idx]);
        out[n][15] = 0;
        n++;
    }
    if (n == 0) { _snwprintf(out[0], 16, L"%s", HL_NONE); out[0][15] = 0; n = 1; }
    *cnt = n;
}

static void almanac_lookup(const WCHAR *dayGZ, const WCHAR *monthGZ,
                           WCHAR yi[3][16], WCHAR ji[3][16],
                           int *yiN, int *jiN)
{
    char needle[4], mh[3], dh[3];
    const char *right = HL_DAY_YI_JI;
    int di, mi, matched = 0;

    *yiN = *jiN = 0;
    di = jiazi_index(dayGZ);
    mi = jiazi_index(monthGZ);
    if (di < 0 || mi < 0) {
        _snwprintf(yi[0], 16, L"%s", HL_NONE); yi[0][15] = 0; *yiN = 1;
        _snwprintf(ji[0], 16, L"%s", HL_NONE); ji[0][15] = 0; *jiN = 1;
        return;
    }
    hex2(di, dh); hex2(mi, mh);
    needle[0] = dh[0]; needle[1] = dh[1]; needle[2] = '='; needle[3] = 0;

    while (!matched) {
        const char *p = strstr(right, needle);
        const char *eq, *colon, *comma;
        int leftLen, monthsLen, i;
        if (!p) break;
        right = p + 3;                    /* 跳过 "DD=" */
        eq = strchr(right, '=');
        leftLen = eq ? (int)(eq - right) - 2 : (int)strlen(right);
        if (leftLen < 0) leftLen = 0;
        colon = memchr(right, ':', (size_t)leftLen);
        if (!colon) { right = eq ? eq - 2 : right + leftLen; continue; }
        monthsLen = (int)(colon - right);

        for (i = 0; i + 1 < monthsLen; i += 2)
            if (right[i] == mh[0] && right[i + 1] == mh[1]) { matched = 1; break; }
        if (!matched) { right = eq ? eq - 2 : right + leftLen; continue; }

        comma = memchr(colon + 1, ',', (size_t)(leftLen - (int)(colon - right) - 1));
        {
            const char *ys = colon + 1;
            int ysLen = comma ? (int)(comma - ys) : leftLen - (int)(colon - right) - 1;
            const char *js = comma ? comma + 1 : NULL;
            int jsLen = comma ? leftLen - (int)(js - right) : 0;
            if (ysLen < 0) ysLen = 0;
            if (jsLen < 0) jsLen = 0;
            take_words(ys, ysLen, yi, yiN);
            if (js) take_words(js, jsLen, ji, jiN);
            else { _snwprintf(ji[0], 16, L"%s", HL_NONE); ji[0][15] = 0; *jiN = 1; }
        }
    }
    if (*yiN == 0) { _snwprintf(yi[0], 16, L"%s", HL_NONE); yi[0][15] = 0; *yiN = 1; }
    if (*jiN == 0) { _snwprintf(ji[0], 16, L"%s", HL_NONE); ji[0][15] = 0; *jiN = 1; }
}

static void fill_terms(int y, int m, int d, LunarInfo *o)
{
    long today = daynum(y, m, d);
    long nextDay = 0;
    int n, cur = -1, haveNext = 0;
    LUNAR_DATE td;

    o->curTerm[0] = 0;
    o->nextTerm[0] = 0;
    o->termToday = 0;
    o->daysToNextTerm = 0;

    /* 库的节气序号：0=小寒 ... 20=立冬 21=小雪 ... */
    for (n = 0; n < 24; n++) {
        const char *name = get_day_of_solar_term((uint32_t)y, (uint8_t)n, &td);
        long t;
        if (!name) break;
        t = daynum(td.year, td.month, td.day);
        if (t <= today) {
            cur = n;
            o->termToday = (t == today) ? 1 : 0;
            u8_to_w(name, o->curTerm, 8);
        } else {
            u8_to_w(name, o->nextTerm, 8);
            nextDay = t;
            haveNext = 1;
            break;
        }
    }
    if (cur < 0) {                       /* 年初小寒之前：上一节气是去年冬至 */
        u8_to_w("冬至", o->curTerm, 8);
    }
    if (!haveNext && y + 1 <= 2099) {    /* 冬至之后：下一个是明年小寒 */
        const char *name = get_day_of_solar_term((uint32_t)(y + 1), 0, &td);
        if (name) {
            u8_to_w(name, o->nextTerm, 8);
            nextDay = daynum(td.year, td.month, td.day);
            haveNext = 1;
        }
    }
    if (haveNext) {
        long diff = nextDay - today;
        o->daysToNextTerm = (diff > 0) ? (int)diff : 0;
    }
}

int lunar_get(int y, int m, int d, LunarInfo *o)
{
    LUNAR_DATE ld;
    const char *s;
    WCHAR dayGZ[8], monthGZ[8];

    memset(o, 0, sizeof(*o));
    if (y < 1900 || y > 2099 || m < 1 || m > 12 || d < 1 || d > 31)
        return 0;

    get_lunar_date((int32_t)y, (uint8_t)m, (uint8_t)d, &ld);
    o->valid = 1;
    o->lunarYear  = ld.year;
    o->lunarMonth = ld.month;
    o->lunarDay   = ld.day;
    o->isLeap     = ld.ext.is_leap_month ? 1 : 0;

    u8_to_w(get_lunar_month_str(ld.month, ld.ext.is_leap_month),
            o->monthCn, 16);
    u8_to_w(get_lunar_day_str(ld.day), o->dayCn, 16);
    u8_to_w(get_year_ganzhi_str(ld.year),  o->yearGZ, 8);
    u8_to_w(get_animal_str(ld.year), o->animal, 8);

    /* 日柱用库的甲戌日锚点； 月柱用节气月(月建)，与通书表口径一致 */
    u8_to_w(get_day_ganzhi_str((int32_t)y, (uint8_t)m, (uint8_t)d),
            dayGZ, 8);
    _snwprintf(o->dayGZ, 8, L"%s", dayGZ); o->dayGZ[7] = 0;
    month_pillar_by_term(y, m, d, monthGZ, 8);
    _snwprintf(o->monthGZ, 8, L"%s", monthGZ); o->monthGZ[7] = 0;

    /* 每日宜忌：各取前 3 条 */
    almanac_lookup(dayGZ, monthGZ, o->yi, o->ji, &o->yiN, &o->jiN);

    
    s = get_gre_festival((uint8_t)m, (uint8_t)d);
    if (!s) s = get_lunar_festival(&ld);
    if (s) {
        u8_to_w(s, o->festival, 32);
        if (wcslen(o->festival) > 10) o->festival[10] = 0;
    }

    fill_terms(y, m, d, o);
    return 1;
}
