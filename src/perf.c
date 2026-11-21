/* perf.c — 性能监测卡片：官方 API 采集 + 自绘圆环/数字/硬件信息 */
#include "perf.h"
#include "app.h"
#include "config.h"
#include "canvas.h"
#include "font.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* 小工具 */
static ULONGLONG ft2u(const FILETIME *ft)
{
    ULARGE_INTEGER u;
    u.LowPart = ft->dwLowDateTime;
    u.HighPart = ft->dwHighDateTime;
    return (ULONGLONG)u.QuadPart;
}
static double gb(ULONGLONG bytes) { return (double)bytes / (1024.0*1024.0*1024.0); }

/* CPU 名称里的 "(R)" "(TM)" 和连续空格很占宽度，压缩一下 */
static void tidy_cpu(WCHAR *dst, const WCHAR *src, int cap)
{
    int i, w = 0, space = 0;
    for (i = 0; src[i] && w < cap - 1; i++) {
        WCHAR ch = src[i];
        if (ch == L'(' && (src[i+1] == L'R' || src[i+1] == L'T' || src[i+1] == L'C') &&
            src[i+2] == L')') { i += 2; continue; }
        if (ch == L' ' || ch == L'\t') { space = 1; continue; }
        if (space && w > 0) { dst[w++] = L' '; space = 0; }
        dst[w++] = ch;
    }
    dst[w] = 0;
}

/* 按像素宽度截断并加省略号，保证硬件信息绝不画出右区 */
static void fit_line(FontSet *f, WCHAR *dst, const WCHAR *src,
                     int px, double maxW)
{
    int n = (int)wcslen(src), i, ellipsis;
    if (font_measure(f, src, n, px, 2) <= maxW) {
        wcsncpy(dst, src, PERF_STR - 1);
        dst[PERF_STR - 1] = 0;
        return;
    }
    ellipsis = font_measure(f, L"…", 1, px, 2);
    for (i = n; i > 1; i--) {
        if (font_measure(f, src, i, px, 2) + ellipsis <= maxW) break;
    }
    if (i < 1) i = 1;
    wcsncpy(dst, src, i);
    dst[i] = L'…';
    dst[i + 1] = 0;
}

static void os_name(Perf *p)
{
    OSVERSIONINFOEXW os;
    SYSTEM_INFO si;
    const WCHAR *base = L"Windows";
    WCHAR sp[24] = L"", arch[16] = L"x86";
    ZeroMemory(&os, sizeof(os));
    os.dwOSVersionInfoSize = sizeof(os);
    if (GetVersionExW((LPOSVERSIONINFOW)&os)) {
        p->osBuild = os.dwBuildNumber;
        if (os.dwMajorVersion == 6 && os.dwMinorVersion == 1)
            base = (os.wProductType == VER_NT_WORKSTATION) ? L"Windows 7" : L"Windows Server 2008 R2";
        else if (os.dwMajorVersion == 6 && os.dwMinorVersion == 2)
            base = (os.wProductType == VER_NT_WORKSTATION) ? L"Windows 8" : L"Windows Server 2012";
        else if (os.dwMajorVersion == 6 && os.dwMinorVersion == 3)
            base = (os.wProductType == VER_NT_WORKSTATION) ? L"Windows 8.1" : L"Windows Server 2012 R2";
        else if (os.dwMajorVersion == 10)
            base = (os.wProductType == VER_NT_WORKSTATION) ? L"Windows 10" : L"Windows Server 2016+";
        else if (os.dwMajorVersion == 5 && os.dwMinorVersion == 1)
            base = L"Windows XP";
        if (os.wServicePackMajor > 0)
            _snwprintf(sp, 24, L" SP%d", os.wServicePackMajor);
        sp[23] = 0;
    }
    ZeroMemory(&si, sizeof(si));
    GetNativeSystemInfo(&si);
    if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ||
        si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_IA64 ||
        si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64) {
        wcscpy(arch, L"x64"); p->bits = 64;
    } else { p->bits = 32; }
    _snwprintf(p->osName, PERF_STR, L"%s%s %s · build %lu",
               base, sp, arch, (unsigned long)p->osBuild);
    p->osName[PERF_STR - 1] = 0;
}

/* 启动时一次性采集硬件 */
static void collect_hw(Perf *p)
{
    HKEY hk = NULL;
    WCHAR raw[256];
    DWORD type = 0, size = sizeof(raw);
    SYSTEM_INFO si;
    MEMORYSTATUSEX ms;
    DISPLAY_DEVICEW dd;
    WCHAR windir[MAX_PATH];
    ULARGE_INTEGER freeA, totalB, freeB;

    /* CPU 名称 */
    wcscpy(p->cpuName, L"Unknown CPU");
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                      0, KEY_READ, &hk) == ERROR_SUCCESS) {
        ZeroMemory(raw, sizeof(raw));
        size = sizeof(raw);
        if (RegQueryValueExW(hk, L"ProcessorNameString", NULL, &type,
                             (LPBYTE)raw, &size) == ERROR_SUCCESS && raw[0])
            tidy_cpu(p->cpuName, raw, PERF_STR);
        RegCloseKey(hk);
    }

    /* 逻辑处理器数 */
    ZeroMemory(&si, sizeof(si));
    GetSystemInfo(&si);
    p->cores = si.dwNumberOfProcessors > 0 ? (int)si.dwNumberOfProcessors : 1;

    /* 物理内存总量 */
    ZeroMemory(&ms, sizeof(ms));
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms))
        p->memTotalGB = gb((ULONGLONG)ms.ullTotalPhys);
    else p->memTotalGB = 0.0;

    /* 显卡：lpDevice=NULL 枚举适配器，DeviceString 即名称 */
    ZeroMemory(&dd, sizeof(dd));
    dd.cb = sizeof(dd);
    if (EnumDisplayDevicesW(NULL, 0, &dd, 0) && dd.DeviceString[0])
        wcsncpy(p->gpuName, dd.DeviceString, PERF_STR - 1);
    else wcscpy(p->gpuName, L"Unknown GPU");
    p->gpuName[PERF_STR - 1] = 0;

    /* 系统盘（取 Windows 目录前 3 个字符，如 "C:\"） */
    wcscpy(p->drive, L"C:\\");
    if (GetSystemWindowsDirectoryW(windir, MAX_PATH) >= 3) {
        p->drive[0] = windir[0]; p->drive[1] = L':'; p->drive[2] = L'\\';
        p->drive[3] = 0;
    }
    if (GetDiskFreeSpaceExW(p->drive, &freeA, &totalB, &freeB)) {
        p->diskTotalGB = gb((ULONGLONG)totalB.QuadPart);
        p->diskFreeGB = gb((ULONGLONG)freeB.QuadPart);
    } else { p->diskTotalGB = 0.0; p->diskFreeGB = 0.0; }

    os_name(p);
}

/* 实时采样 */
static void sample(Perf *p)
{
    FILETIME fi, fk, fu;
    ULONGLONG total, idle, dt, di;
    MEMORYSTATUSEX ms;
    ULARGE_INTEGER freeA, totalB, freeB;
    WCHAR tmp[PERF_STR * 2];

    /* CPU：官方算法 total=kernel+user（kernel 含 idle），busy=total-idle */
    if (GetSystemTimes(&fi, &fk, &fu)) {
        total = ft2u(&fk) + ft2u(&fu);
        idle  = ft2u(&fi);
        if (p->haveBaseline) {
            dt = total - p->prevTotal;
            di = idle - p->prevIdle;
            if (dt > 0) {
                double v = 100.0 * (double)(dt - di) / (double)dt;
                p->cpu = qs_clamp(v, 0.0, 100.0);
            }
        }
        p->prevTotal = total;
        p->prevIdle = idle;
        p->haveBaseline = 1;
    }

    /* 内存：dwMemoryLoad 直接就是 0..100 */
    ZeroMemory(&ms, sizeof(ms));
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) {
        p->mem = qs_clamp((double)ms.dwMemoryLoad, 0.0, 100.0);
        p->memTotalGB = gb((ULONGLONG)ms.ullTotalPhys);
        p->memUsedGB = p->memTotalGB - gb((ULONGLONG)ms.ullAvailPhys);
        if (p->memUsedGB < 0.0) p->memUsedGB = 0.0;
    }

    /* 系统盘占用 */
    if (GetDiskFreeSpaceExW(p->drive, &freeA, &totalB, &freeB)) {
        double t = gb((ULONGLONG)totalB.QuadPart);
        double fr = gb((ULONGLONG)freeB.QuadPart);
        p->diskTotalGB = t; p->diskFreeGB = fr;
        p->disk = (t > 0.0) ? qs_clamp(100.0 * (t - fr) / t, 0.0, 100.0) : 0.0;
    }

    /* 重建右侧硬件信息行（宽度截断在绘制时按实时字号做，这里只生成全文） */
    _snwprintf(tmp, PERF_STR * 2, L"%s", p->cpuName);
    wcsncpy(p->hwLines[0], tmp, PERF_STR - 1); p->hwLines[0][PERF_STR - 1] = 0;

    _snwprintf(p->hwLines[1], PERF_STR, L"%d 核 · %.1f GB 内存 · 已用 %.1f GB",
               p->cores, p->memTotalGB, p->memUsedGB);
    p->hwLines[1][PERF_STR - 1] = 0;

    _snwprintf(tmp, PERF_STR * 2, L"%s", p->gpuName);
    wcsncpy(p->hwLines[2], tmp, PERF_STR - 1); p->hwLines[2][PERF_STR - 1] = 0;

    wcsncpy(p->hwLines[3], p->osName, PERF_STR - 1); p->hwLines[3][PERF_STR - 1] = 0;

    /* 用户要求「性能监测不用写 C 盘大小和占用」：右侧硬件信息不再输出 系统盘容量 / 可用空间那一整行。 磁盘占用率仍由第三个圆环显示 （那是用户最初点名的三环之一，不删）。 */

    p->sampleCount++;
}

void perf_init(Perf *p)
{
    ZeroMemory(p, sizeof(*p));
    collect_hw(p);
    sample(p);                 /* 立刻给出内存/磁盘环，CPU 等第二次差分 */
    p->lastSample = qs_now();
}

void perf_shutdown(Perf *p) { (void)p; }

int perf_tick(Perf *p, DWORD now)
{
    if (p->lastSample && (DWORD)(now - p->lastSample) < PERF_SAMPLE_MS) return 0;
    p->lastSample = now;
    sample(p);
    return 1;
}

void perf_force_sample(Perf *p)
{
    sample(p);
    p->lastSample = qs_now();
}

void perf_hw_summary(const Perf *p, char *out, int cap)
{
    char a[512], b[512], c[512];
    WideCharToMultiByte(CP_UTF8, 0, p->cpuName, -1, a, 512, NULL, NULL);
    WideCharToMultiByte(CP_UTF8, 0, p->gpuName, -1, b, 512, NULL, NULL);
    WideCharToMultiByte(CP_UTF8, 0, p->osName, -1, c, 512, NULL, NULL);
    _snprintf(out, cap, "cpu=%s | cores=%d | mem=%.1fGB | gpu=%s | os=%s | drive=%ls %.1fGB",
              a, p->cores, p->memTotalGB, b, c, p->drive, p->diskTotalGB);
    out[cap - 1] = 0;
}

void perf_ring_rgb(int idx, double val, int *r, int *g, int *b)
{
    static const int base[3][3] = {
        { PERF_CPU_R,  PERF_CPU_G,  PERF_CPU_B  },
        { PERF_MEM_R,  PERF_MEM_G,  PERF_MEM_B  },
        { PERF_DISK_R, PERF_DISK_G, PERF_DISK_B }
    };
    double t, s;
    int i = qs_iclamp(idx, 0, 2);
    t = qs_clamp((val - PERF_WARN_PCT) / (100.0 - PERF_WARN_PCT), 0.0, 1.0);
    s = t * t * (3.0 - 2.0 * t);          
    *r = (int)(base[i][0] + (PERF_WARN_R - base[i][0]) * s + 0.5);
    *g = (int)(base[i][1] + (PERF_WARN_G - base[i][1]) * s + 0.5);
    *b = (int)(base[i][2] + (PERF_WARN_B - base[i][2]) * s + 0.5);
}

/* 圆环：2x2 子采样面积覆盖率，无锯齿 颜色只作为 canvas_blend_px 的源色传入，覆盖率算法完全不动： 彩色不会带来任何锯齿，边缘仍是连续灰阶。 */
static void draw_ring(Canvas *c, double cx, double cy, double rad, double th,
                      double val, double k, int idx)
{
    const int ss = PERF_RING_SS;
    double outer = rad + th * 0.5, inner = rad - th * 0.5;
    double soft = 1.0;                 /* 输出像素单位的边缘过渡 */
    double prog = 2.0 * M_PI * qs_clamp(val / 100.0, 0.0, 1.0);
    int full = (val >= 99.95), empty = (val <= 0.02);
    int x0, y0, x1, y1, x, y;
    int br, bg, bb;

    perf_ring_rgb(idx, val, &br, &bg, &bb);

    x0 = qs_iclamp((int)floor(cx - outer - 2), 0, c->W);
    y0 = qs_iclamp((int)floor(cy - outer - 2), 0, c->H);
    x1 = qs_iclamp((int)ceil(cx + outer + 2), 0, c->W);
    y1 = qs_iclamp((int)ceil(cy + outer + 2), 0, c->H);

    for (y = y0; y < y1; y++) {
        for (x = x0; x < x1; x++) {
            double covBg = 0.0, covFg = 0.0;
            double shade = 1.0;
            int sy, sx;
            /* 沿弧明暗：顶部起点偏暗，顺时针渐亮到末端，给平面色一点体积感。 用输出像素中心的角度即可，不参与覆盖率计算。 */
            {
                double dx = (x + 0.5) - cx, dy = (y + 0.5) - cy;
                double ang = atan2(dx, -dy);
                if (ang < 0.0) ang += 2.0 * M_PI;
                double span = full ? 2.0 * M_PI : (prog > 0.0 ? prog : 1.0);
                double t = qs_clamp(ang / span, 0.0, 1.0);
                shade = PERF_ARC_SHADE + (1.0 - PERF_ARC_SHADE) * t;
            }
            for (sy = 0; sy < ss; sy++) {
                for (sx = 0; sx < ss; sx++) {
                    double px = x + ((double)sx + 0.5) / (double)ss;
                    double py = y + ((double)sy + 0.5) / (double)ss;
                    double dx = px - cx, dy = py - cy;
                    double d = sqrt(dx * dx + dy * dy);
                    double band = qs_clamp((outer - d) / soft + 0.5, 0.0, 1.0) *
                                  qs_clamp((d - inner) / soft + 0.5, 0.0, 1.0);
                    double pc, ang;
                    if (band <= 0.0) continue;
                    if (full) pc = 1.0;
                    else if (empty) pc = 0.0;
                    else {
                        /* atan2(dx,-dy)：顶部=0，顺时针递增到 2π */
                        ang = atan2(dx, -dy);
                        if (ang < 0.0) ang += 2.0 * M_PI;
                        pc = qs_clamp((prog - ang) / PERF_ARC_SOFT + 0.5, 0.0, 1.0);
                    }
                    covBg += band * (1.0 - pc);
                    covFg += band * pc;
                }
            }
            covBg /= (double)(ss * ss);
            covFg /= (double)(ss * ss);
            if (covBg > 0.0)
                canvas_blend_px(c, x, y, PERF_RING_BG_R, PERF_RING_BG_G,
                                PERF_RING_BG_B, covBg * k);
            if (covFg > 0.0)
                canvas_blend_px(c, x, y,
                                (int)(br * shade + 0.5),
                                (int)(bg * shade + 0.5),
                                (int)(bb * shade + 0.5),
                                covFg * k);
        }
    }
}

/* 整页绘制 */
void perf_draw(Perf *p, App *a, double W, double H, double k)
{
    Canvas *c = &a->canvas;
    FontSet *f = &a->fonts;
    double L = ((double)c->W - W) * 0.5;
    double rad = qs_clamp(H * 0.245, 20.0, 46.0);
    double th  = rad * 0.20;
    double gap = W * 0.026;
    double cy  = H * 0.445;
    double x0  = L + W * 0.112;
    double hx  = L + W * 0.615, hx1 = L + W * 0.972;
    double maxW = hx1 - hx;
    /* 用户要求「font size 放大一点」：环心数字 / 环下标签 / 页标题 / 硬件信息四档一起抬一档，clamp 上限同步放宽。 */
    int ringPx = qs_iclamp((int)(rad * 0.70), 14, 32);
    int labelPx = qs_iclamp((int)(H * 0.108), 12, 21);
    int titlePx = qs_iclamp((int)(H * 0.128), 14, 26);
    int hwPx = qs_iclamp((int)(H * 0.112), 12, 22);
    static const WCHAR *labels[3] = { L"CPU", L"内存", L"磁盘" };
    double vals[3];
    WCHAR num[16];
    int i;

    vals[0] = p->cpu; vals[1] = p->mem; vals[2] = p->disk;

    /* 三个圆环 + 环心百分比数字 + 下方文字标签 */
    for (i = 0; i < 3; i++) {
        double cx = x0 + rad + (double)i * (2.0 * rad + gap);
        double ly = H * 0.825;
        int r, g, b, nr, ng, nb;
        perf_ring_rgb(i, vals[i], &r, &g, &b);
        
        nr = (int)(r + (255 - r) * PERF_NUM_MIX + 0.5);
        ng = (int)(g + (255 - g) * PERF_NUM_MIX + 0.5);
        nb = (int)(b + (255 - b) * PERF_NUM_MIX + 0.5);
        draw_ring(c, cx, cy, rad, th, vals[i], k, i);
        _snwprintf(num, 16, L"%d%%", (int)(vals[i] + 0.5));
        font_draw_text(f, c, num, ringPx, cx, cy, 1, nr, ng, nb, k);
        font_draw_label(f, c, labels[i], labelPx, cx, ly, 1,
                        DIM_R, DIM_G, DIM_B, k * 0.92);
    }

    /* 右侧：标题 + 4 行硬件信息（C 盘容量行已删）。 本页放开文字政策。 */
    font_draw_label(f, c, L"性能监测", titlePx, hx, H * 0.145, 0,
                    ACCENT_R, ACCENT_G, ACCENT_B, k);
    for (i = 0; i < PERF_HW_LINES; i++) {
        WCHAR line[PERF_STR];
        double y = H * 0.315 + (double)i * (H * 0.170);
        fit_line(f, line, p->hwLines[i], hwPx, maxW);
        font_draw_label(f, c, line, hwPx, hx, y, 0,
                        DIM_R, DIM_G, DIM_B, k * 0.95);
    }
}
