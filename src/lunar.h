/* lunar.h — 本地农历库 */
#ifndef QISLAND_LUNAR_H
#define QISLAND_LUNAR_H

#include "common.h"

typedef struct {
    int   valid;            /* 1 = 库覆盖范围内，数据可用 */
    int   lunarYear;
    int   lunarMonth;
    int   lunarDay;
    int   isLeap;
    WCHAR monthCn[16];      /* 十月 / 闰二月（库已含「闰」和「月」） */
    WCHAR dayCn[16];        /* 初九 / 二十 / 三十 */
    WCHAR yearGZ[8];        /* 丙午 */
    WCHAR monthGZ[8];       /* 己亥 */
    WCHAR dayGZ[8];         /* 乙未 */
    WCHAR animal[8];        /* 马 */
    WCHAR festival[32];     /* 当天节日/纪念日，无则空 */
    WCHAR curTerm[8];       /* 立冬：只在交节当天显示 */
    WCHAR nextTerm[8];      /* 小雪 */
    int   termToday;        /* 1 = 今天正是某个节气的交节日 */
    int   daysToNextTerm;

    WCHAR yi[3][16];
    WCHAR ji[3][16];
    int   yiN, jiN;
} LunarInfo;

int lunar_get(int y, int m, int d, LunarInfo *out);

#endif /* QISLAND_LUNAR_H */
