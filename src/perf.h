/* perf.h — 第三个中卡片（view 2）：性能监测 */
#ifndef QISLAND_PERF_H
#define QISLAND_PERF_H

#include "common.h"

#define PERF_HW_LINES   4       /* 用户要求：不写 C 盘大小和占用，删掉原本第 5 行 */
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
/* 到点才采样； 返回 1 表示产生了新样本（调用方据此置脏重绘） */
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
