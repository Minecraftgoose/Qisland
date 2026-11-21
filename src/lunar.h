/* lunar.h — 本地农历库封装（无网络、无线程、无缓存、无状态）
 *
 * 历法数据由 vendored 纯 C 库提供：
 *   src/vendor/lunar_date.c/.h + solar_term.c/.h
 *   upstream https://github.com/iseemoon/lunar-calendar  (Apache-2.0)
 *   覆盖公历 1900..2099，24 节气是逐年精确 UTC 毫秒表，不是近似公式。
 *
 * 本文件只暴露一个同步查询：给定公历年月日，一次性返回日期卡需要的
 * 全部历法字符串（UTF-8 → WCHAR 在本模块边界完成）。调用方每次绘制
 * 直接取，不存在「请求成功那一帧」的概念，也就没有 serial / 置脏 /
 * 后台线程 / 超时 / 回退分支。静止零重绘策略完全不受影响。
 */
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

    /* 每日宜忌：各取前 3 条，来自 vendored 通书表（MIT），离线查表。
       月柱用「节气月(月建)」口径，不是农历月 —— 与上游 6tail 一致。 */
    WCHAR yi[3][16];
    WCHAR ji[3][16];
    int   yiN, jiN;
} LunarInfo;

/* 同步查询。返回 valid；超出 1900..2099 时清零并返回 0。 */
int lunar_get(int y, int m, int d, LunarInfo *out);

#endif /* QISLAND_LUNAR_H */
