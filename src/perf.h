/* perf.h — 第三个中卡片（view 2）：性能监测
 *
 * 设计：
 *   · 三个圆环（CPU / 内存 / 磁盘）+ 环心百分比数字 + 下方文字标签。
 *     圆环用逐像素 2x2 面积覆盖率，扇形端点做角度软化，无锯齿。
 *   · 右侧是硬件信息文字。本页是唯一放开文字政策的一页（用户明确允许），
 *     其余页面仍只有「倒计时」三个字。
 *   · 实时值不用 PDH：GetSystemTimes 差分算 CPU、GlobalMemoryStatusEx 读
 *     内存、GetDiskFreeSpaceEx 读系统盘。全部是 Win7 自带 kernel32 API，
 *     不需要 pdh.lib / COM / 本地化计数器名。
 *   · 硬件信息只在启动时采一次；实时值每 PERF_SAMPLE_MS 采样一次。
 *     只有当前视图是 PERF_VIEW 且卡片展开时才采样，静止零重绘不受影响。
 */
#ifndef QISLAND_PERF_H
#define QISLAND_PERF_H

#include "common.h"

#define PERF_HW_LINES   5
#define PERF_STR        128

typedef struct {
    /* 实时百分比 0..100 */
    double cpu, mem, disk;
    /* 具体容量（GB），用于硬件信息行 */
    double memTotalGB, memUsedGB;
    double diskTotalGB, diskFreeGB;

    /* 启动时一次性采集的硬件信息 */
    WCHAR  cpuName[PERF_STR];
    WCHAR  gpuName[PERF_STR];
    WCHAR  osName[PERF_STR];
    WCHAR  drive[8];
    int    cores;
    int    bits;
    DWORD  osBuild;

    /* GetSystemTimes 差分基准 */
    ULONGLONG prevTotal, prevIdle;
    int    haveBaseline;
    DWORD  lastSample;
    int    sampleCount;

    /* 右侧展示行，每次采样重建（磁盘/内存数值会变） */
    WCHAR  hwLines[PERF_HW_LINES][PERF_STR];
} Perf;

void perf_init(Perf *p);
void perf_shutdown(Perf *p);
/* 到点才采样；返回 1 表示产生了新样本（调用方据此置脏重绘） */
int  perf_tick(Perf *p, DWORD now);
/* 测试用：立即强制采样一次 */
void perf_force_sample(Perf *p);

/* 绘制：W/H 是设备像素（已乘 DPI），k 是展开淡入系数 */
void perf_draw(Perf *p, App *a, double W, double H, double k);

/* 供诊断：把硬件信息拼成一行 UTF-8（调用者传缓冲区） */
void perf_hw_summary(const Perf *p, char *out, int cap);
/* 供诊断：第 idx 个环在 val 下的实际 RGB（与绘制同一套插值） */
void perf_ring_rgb(int idx, double val, int *r, int *g, int *b);

#endif /* QISLAND_PERF_H */
