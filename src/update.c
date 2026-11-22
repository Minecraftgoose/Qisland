/* update.c — 设置页「更新」：查 GitHub 最新 Release，回报版本与原始响应 */
#include "update.h"
#include "app.h"
#include "config.h"
#include "version.h"
#include "notify.h"
#include "icons.h"

#include <wininet.h>       /* 本翻译单元只用 WinINet； WinHTTP 在另一个 TU */
#include <wchar.h>
#include <stdio.h>

/* 常量 */
#ifndef UPD_TIMEOUT_MS
#define UPD_TIMEOUT_MS   15000
#endif
#ifndef UPD_CONNECT_MS
#define UPD_CONNECT_MS   20000
#endif
#ifndef UPD_HARD_MS
#define UPD_HARD_MS      30000
#endif
#ifndef UPD_MAX_BODY
#define UPD_MAX_BODY     (1024 * 1024)
#endif

static const WCHAR *UPD_HEADERS =
    L"Accept: application/vnd.github+json\r\n"
    L"User-Agent: Qisland/" QI_VERSION_W L"\r\n";

#define EXCHANGE(p, v)  InterlockedExchange((volatile LONG *)(p), (LONG)(v))
#define READ(p)         InterlockedCompareExchange((volatile LONG *)(p), 0, 0)

/* 结果缓冲 */
/* 只有主线程读写：工作线程通过 WM_APP_UPDATE 把一份独立内存交过来 */
static UpdMsg   g_result;
static double   g_scroll = 0.0;
static double   g_doch   = 0.0;

static volatile LONG g_busy     = 0;
static volatile LONG g_shutdown = 0;

/* 按钮状态与下载进度只由主线程写，绘制和点击分派读同一份 */
static int   g_state = UPD_ST_IDLE;
static int   g_permille = 0;
static DWORD g_done = 0;
static char  g_newTag[64];
static WCHAR g_setup[1024];
static int   g_armed = 0;

int  update_busy(void) { return (int)READ(&g_busy); }
int  update_result_kind(void) { return g_result.kind; }
const WCHAR *update_result_text(void) { return g_result.text; }
double update_result_scroll(void) { return g_scroll; }
void update_result_set_doch(double h, double viewH)
{
    double maxS = h - viewH;
    g_doch = h;
    if (maxS < 0.0) maxS = 0.0;
    if (g_scroll > maxS) g_scroll = maxS;
    if (g_scroll < 0.0) g_scroll = 0.0;
}
void update_result_reset_scroll(void) { g_scroll = 0.0; }

void update_result_wheel(int dir, double viewH)
{
    double maxS = g_doch - viewH;
    double s;
    if (maxS < 0.0) maxS = 0.0;
    s = g_scroll + (double)dir * viewH * 0.22;
    if (s < 0.0) s = 0.0;
    if (s > maxS) s = maxS;
    g_scroll = s;
}

/* 小工具 */
static void tag_to_ui(const char *tag, WCHAR *out, int cap);
/* "v1.10.2" / "1.10" / "v2" / "v1.2.3.4" → 最多四段数字； 返回 0 = 解析不出来 */
static int parse_ver(const char *t, int *maj, int *min, int *pat, int *rev)
{
    int v[4] = { 0, 0, 0, 0 }, n = 0, i;
    while (*t == ' ' || *t == 'v' || *t == 'V' || *t == '\t') t++;
    if (*t < '0' || *t > '9') return 0;
    for (i = 0; i < 4; i++) {
        int x = 0;
        if (*t < '0' || *t > '9') break;
        while (*t >= '0' && *t <= '9') {
            x = x * 10 + (*t - '0');
            if (x > 99999) x = 99999;
            t++;
        }
        v[i] = x; n = i + 1;
        if (*t != '.') break;
        t++;
    }
    *maj = v[0]; *min = v[1]; *pat = v[2]; *rev = v[3];
    return n > 0;
}

static int ver_cmp(int a0, int a1, int a2, int a3, int b0, int b1, int b2, int b3)
{
    if (a0 != b0) return a0 < b0 ? -1 : 1;
    if (a1 != b1) return a1 < b1 ? -1 : 1;
    if (a2 != b2) return a2 < b2 ? -1 : 1;
    if (a3 != b3) return a3 < b3 ? -1 : 1;
    return 0;
}

/* JSON 里找 "key":"value"，简单跳过转义； 找不到返回 0 */
static int json_str(const char *json, const char *key, char *out, int outn)
{
    char pat[128];
    int k = _snprintf(pat, sizeof(pat), "\"%s\":\"", key);
    const char *p;
    int i = 0;
    if (k <= 0 || k >= (int)sizeof(pat)) return 0;
    p = strstr(json, pat);
    if (!p) return 0;
    p += k;
    while (*p && *p != '"' && i < outn - 1) {
        if (*p == '\\' && p[1]) p++;
        out[i++] = *p++;
    }
    out[i] = 0;
    return i > 0;
}

/* 文本拼装 */
static void text_cat(WCHAR *dst, int cap, const WCHAR *s)
{
    int i = 0, j = 0;
    while (dst[i]) i++;
    while (s[j] && i + 1 < cap) dst[i++] = s[j++];
    dst[i] = 0;
}

static void make_brief(UpdMsg *m)
{
    int i = 0, j = 0;
    while (m->text[i] && j < UPD_BRIEF_CHARS - 1) {
        WCHAR ch = m->text[i++];
        if (ch == L'\r' || ch == L'\n' || ch == L'\t') ch = L' ';
        else if (ch < 0x20 || ch == 0x7f) ch = L'.';
        m->brief[j++] = ch;
    }
    m->brief[j] = 0;
}

static void write_raw_file(const char *body, int len, WCHAR *path, int pathn)
{
    WCHAR dir[1024];
    HANDLE h;
    DWORD wr = 0;
    path[0] = 0;
    if (!GetTempPathW(1024, dir)) return;
    _snwprintf(path, pathn, L"%s%s", dir, UPD_RAW_FILE);
    path[pathn - 1] = 0;
    h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { path[0] = 0; return; }
    if (len > 0) WriteFile(h, body, (DWORD)len, &wr, NULL);
    CloseHandle(h);
}

static void delete_raw_file(void)
{
    WCHAR dir[1024], path[1024];
    if (!GetTempPathW(1024, dir)) return;
    _snwprintf(path, 1024, L"%s%s", dir, UPD_RAW_FILE);
    path[1023] = 0;
    DeleteFileW(path);
}

/* 失败：只给事实 + 服务器返回的原始字节，不猜原因 */
static void set_raw(UpdMsg *m, const WCHAR *stage, DWORD status,
                    const char *body, int len, const char *url,
                    DWORD winerr, const WCHAR *winext)
{
    WCHAR t[1200], wu[1024], path[1024];
    int hasBody = (body && len > 0);
    int wn = 0, srcN, shown = 0;
    WCHAR *wide, *show;

    m->kind   = UPD_KIND_ERROR;
    m->status = status;
    wcscpy(m->title, L"检查更新失败");
    m->text[0] = 0;

    _snwprintf(t, 1200, L"[阶段] %s\n", stage);             text_cat(m->text, UPD_TEXT_CHARS, t);
    if (status) {
        _snwprintf(t, 1200, L"[HTTP] %lu\n", (unsigned long)status);
        text_cat(m->text, UPD_TEXT_CHARS, t);
    }
    /* 没有响应体时连 0 也显示：系统层没报错本身就是一条事实 */
    if (winerr || !hasBody) {
        _snwprintf(t, 1200, L"[Win32] %lu\n", (unsigned long)winerr);
        text_cat(m->text, UPD_TEXT_CHARS, t);
    }
    if (url && *url) {
        if (MultiByteToWideChar(CP_UTF8, 0, url, -1, wu, 1024)) {
            _snwprintf(t, 1200, L"[URL] %s\n", wu);
            text_cat(m->text, UPD_TEXT_CHARS, t);
        }
    }
    if (winext && *winext) {
        _snwprintf(t, 1200, L"[WinINet] %s\n", winext);
        text_cat(m->text, UPD_TEXT_CHARS, t);
    }

    if (hasBody) {
        _snwprintf(t, 1200, L"[响应 %d 字节]\n", len);
        text_cat(m->text, UPD_TEXT_CHARS, t);

        /* 优先按 UTF-8 解码； 非法字节则退回逐字节 */
        wide = (WCHAR *)malloc(((size_t)len + 1) * sizeof(WCHAR));
        if (wide) {
            wn = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                     body, len, wide, len + 1);
            if (wn <= 0) wn = 0;
        }
        srcN = wn > 0 ? wn : len;
        show = (WCHAR *)malloc((UPD_RAW_SHOW + 2) * sizeof(WCHAR));
        if (show) {
            while (shown < UPD_RAW_SHOW && shown < srcN) {
                WCHAR ch = wn > 0 ? wide[shown]
                                  : (WCHAR)(unsigned char)body[shown];
                if (ch != L'\r' && ch != L'\n' && ch != L'\t' &&
                    (ch < 0x20 || ch == 0x7f)) ch = L'.';
                show[shown++] = ch;
            }
            show[shown] = 0;
            text_cat(m->text, UPD_TEXT_CHARS, show);
            free(show);
        }
        if (wide) free(wide);
        if (srcN > shown) {
            _snwprintf(t, 1200, L"\n[... 已截断，完整响应 %d 字节]\n", len);
            text_cat(m->text, UPD_TEXT_CHARS, t);
        } else {
            text_cat(m->text, UPD_TEXT_CHARS, L"\n");
        }

        write_raw_file(body, len, path, 1024);
        if (*path) {
            _snwprintf(t, 1200, L"[完整响应] %s\n", path);
            text_cat(m->text, UPD_TEXT_CHARS, t);
        }
    } else {
        text_cat(m->text, UPD_TEXT_CHARS, L"[响应] (无响应体)\n");
        
        delete_raw_file();
    }
    make_brief(m);
}

static void set_info(UpdMsg *m, int kind, const WCHAR *title, const WCHAR *text)
{
    m->kind = kind;
    m->status = 200;
    wcsncpy(m->title, title, 63);
    m->title[63] = 0;
    wcsncpy(m->text, text, UPD_TEXT_CHARS - 1);
    m->text[UPD_TEXT_CHARS - 1] = 0;
    make_brief(m);
}

/* HTTP 主链路 */
typedef struct {
    volatile PVOID inet, url, whReq;
    volatile LONG  abort;
} HttpSlots;

static void slots_close(HttpSlots *s, HINTERNET hUrl, HINTERNET hInet)
{
    HINTERNET u = s ? (HINTERNET)InterlockedExchangePointer(&s->url, NULL) : hUrl;
    HINTERNET i = s ? (HINTERNET)InterlockedExchangePointer(&s->inet, NULL) : hInet;
    if (u) InternetCloseHandle(u);
    if (i) InternetCloseHandle(i);
}

/* 失败时 *out 仍可能是服务器返回的原始响应体，由调用方 free */
static int http_get_ex(const char *url, const WCHAR *headers,
                       char **out, int *plen, DWORD *status,
                       HttpSlots *slots, DWORD tmo,
                       DWORD *winerr, WCHAR *winext, int winextN)
{
    HINTERNET hInet, hUrl;
    WCHAR wurl[2048];
    DWORD flags, ct, t, code = 0, sz, got;
    size_t cap = 16384, len = 0;
    char *buf;
    int ok = 1, httpBad, failed;

    *out = NULL; *plen = 0; *status = 0;
    if (winerr) *winerr = 0;
    if (winext && winextN) winext[0] = 0;

    hInet = InternetOpenW(L"Qisland/" QI_VERSION_W,
                          INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInet) { if (winerr) *winerr = GetLastError(); return 0; }
    if (slots) InterlockedExchangePointer(&slots->inet, hInet);

    ct = tmo < UPD_CONNECT_MS ? tmo : UPD_CONNECT_MS;
    t  = tmo;
    InternetSetOption(hInet, INTERNET_OPTION_CONNECT_TIMEOUT,      &ct, sizeof(ct));
    InternetSetOption(hInet, INTERNET_OPTION_SEND_TIMEOUT,         &t,  sizeof(t));
    InternetSetOption(hInet, INTERNET_OPTION_RECEIVE_TIMEOUT,      &t,  sizeof(t));
    InternetSetOption(hInet, INTERNET_OPTION_DATA_SEND_TIMEOUT,    &t,  sizeof(t));
    InternetSetOption(hInet, INTERNET_OPTION_DATA_RECEIVE_TIMEOUT, &t,  sizeof(t));

    if (MultiByteToWideChar(CP_UTF8, 0, url, -1, wurl, 2048) <= 0) {
        if (winerr) *winerr = GetLastError();
        slots_close(slots, NULL, hInet);
        return 0;
    }
    flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE |
            INTERNET_FLAG_NO_UI | INTERNET_FLAG_PRAGMA_NOCACHE |
            INTERNET_FLAG_NO_COOKIES;
    if (strncmp(url, "https://", 8) == 0) flags |= INTERNET_FLAG_SECURE;
    hUrl = InternetOpenUrlW(hInet, wurl, headers, 0, flags, 0);
    if (!hUrl) {
        if (winerr) *winerr = GetLastError();
        if (winext && winextN) {
            DWORD e = 0, n = (DWORD)winextN;
            InternetGetLastResponseInfoW(&e, winext, &n);
        }
        slots_close(slots, NULL, hInet);
        return 0;
    }
    if (slots) InterlockedExchangePointer(&slots->url, hUrl);
    InternetSetOption(hUrl, INTERNET_OPTION_RECEIVE_TIMEOUT,      &t, sizeof(t));
    InternetSetOption(hUrl, INTERNET_OPTION_DATA_RECEIVE_TIMEOUT, &t, sizeof(t));
    InternetSetOption(hUrl, INTERNET_OPTION_SEND_TIMEOUT,         &t, sizeof(t));

    /* 4xx/5xx 也要把错误页读回来：403 的 JSON 里才有真正的原始信息 */
    sz = sizeof(code);
    if (HttpQueryInfoW(hUrl, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                       &code, &sz, NULL)) *status = code;
    httpBad = (code >= 400);
    if (httpBad && winext && winextN) {
        DWORD e = 0, n = (DWORD)winextN;
        InternetGetLastResponseInfoW(&e, winext, &n);
    }

    buf = (char *)malloc(cap + 1);
    if (!buf) { slots_close(slots, hUrl, hInet); return 0; }
    for (;;) {
        if (slots && READ(&slots->abort)) { ok = 0; break; }
        if (len + 4096 + 1 > cap) {
            char *nb;
            size_t nc;
            if (cap >= UPD_MAX_BODY) { ok = 0; break; }
            nc = cap * 2; if (nc > UPD_MAX_BODY) nc = UPD_MAX_BODY;
            nb = (char *)realloc(buf, nc + 1);
            if (!nb) { ok = 0; break; }
            buf = nb; cap = nc;
        }
        got = 0;
        if (!InternetReadFile(hUrl, buf + len, (DWORD)(cap - len - 1), &got)) {
            ok = 0; break;
        }
        if (got == 0) break;
        len += got;
        if (len >= UPD_MAX_BODY) { ok = 0; break; }
    }
    buf[len] = 0;
    /* 连上了但一个字节都没来，也算失败 */
    failed = (!ok || httpBad || len == 0);
    if (failed) {
        if (winerr && !*winerr) *winerr = GetLastError();
        if (winext && winextN && !winext[0]) {
            DWORD e = 0, n = (DWORD)winextN;
            InternetGetLastResponseInfoW(&e, winext, &n);
        }
    }
    slots_close(slots, hUrl, hInet);
    if (failed) {
        if (len > 0) { *out = buf; *plen = (int)len; } else free(buf);
        return 0;
    }
    *out = buf; *plen = (int)len;
    return 1;
}

/* 线程上下文 */
typedef struct {
    HWND         hwnd;
    HANDLE       watch;
    volatile LONG finished, netActive, netStart, timedOut, netKind;
    HttpSlots    slots;
    char         tag[64];
    char         url[1024];      /* 加速地址 */
    char         direct[1024];   /* 加速不可用时回退的官方直链 */
    WCHAR        setup[1024];
} UpdCtx;

/* kind=1 查 API，kind=2 下安装包，看门狗按类型取不同阈值 */
static void net_begin(UpdCtx *c, int kind)
{
    EXCHANGE(&c->netKind, kind);
    EXCHANGE(&c->netStart, (LONG)GetTickCount());
    EXCHANGE(&c->netActive, 1);
}
static void net_end(UpdCtx *c) { EXCHANGE(&c->netActive, 0); }

static void post_result(HWND hwnd, UpdMsg *m)
{
    if (!PostMessageW(hwnd, WM_APP_UPDATE, 0, (LPARAM)m)) free(m);
}

/* 看门狗：超时就关掉正在阻塞读的句柄，把工作线程拽出来 */
static DWORD WINAPI upd_watch(LPVOID arg)
{
    UpdCtx *c = (UpdCtx *)arg;
    for (;;) {
        if (READ(&c->finished) || READ(&g_shutdown)) break;
        if (READ(&c->netActive)) {
            DWORD st = (DWORD)READ(&c->netStart);
            DWORD limit = READ(&c->netKind) == 2 ? UPD_DL_HARD_MS : UPD_HARD_MS;
            if (GetTickCount() - st > limit) {
                if (InterlockedCompareExchange(&c->timedOut, 1, 0) == 0) {
                    UpdTimeoutMsg *tm;
                    PVOID r;
                    int isDl = (limit == UPD_DL_HARD_MS);
                    EXCHANGE(&c->slots.abort, 1);
                    r = InterlockedExchangePointer(&c->slots.url, NULL);
                    if (r) InternetCloseHandle((HINTERNET)r);
                    r = InterlockedExchangePointer(&c->slots.inet, NULL);
                    if (r) InternetCloseHandle((HINTERNET)r);
                    qi_winhttp_abort(&c->slots.whReq);
                    tm = (UpdTimeoutMsg *)calloc(1, sizeof(*tm));
                    if (tm) {
                        tm->isDownload = isDl;
                        _snwprintf(tm->msg, 300,
                            L"[阶段] %s\n[超时] %lu ms\n[Win32] %lu\n"
                            L"[响应] (无响应体)\n",
                            isDl ? L"下载" : L"API",
                            (unsigned long)limit,
                            (unsigned long)GetLastError());
                        if (!PostMessageW(c->hwnd, WM_APP_UPDATE, 1, (LPARAM)tm))
                            free(tm);
                    }
                    /* 立刻允许重试，不等可能永远醒不过来的工作线程 */
                    InterlockedExchange(&g_busy, 0);
                }
                break;
            }
        }
        Sleep(250);
    }
    return 0;
}

static DWORD WINAPI upd_thread(LPVOID arg)
{
    UpdCtx *c = (UpdCtx *)arg;
    char api[512], *json = NULL, *wjson = NULL;
    WCHAR wapi[512], ext[512] = L"";
    int jlen = 0, wlen = 0, ok;
    DWORD st = 0, wst = 0, we = 0;
    UpdMsg *m;

    _snprintf(api, sizeof(api),
              "https://api.github.com/repos/%s/%s/releases/latest",
              QI_REPO_OWNER, QI_REPO_NAME);
    api[sizeof(api) - 1] = 0;
    MultiByteToWideChar(CP_UTF8, 0, api, -1, wapi, 512);

    net_begin(c, 1);
    ok = http_get_ex(api, UPD_HEADERS, &json, &jlen, &st,
                     &c->slots, UPD_TIMEOUT_MS, &we, ext, 512);
    if (!ok && !READ(&c->slots.abort)) {
        /* Win7 的 Schannel 常没开 TLS 1.2，WinINet 直接握手失败； WinHTTP 显式请求 TLS 1.2 再试一次，不依赖外部 exe。 */
        int wok = qi_winhttp_get(wapi, UPD_HEADERS, &wjson, &wlen, &wst,
                                 &c->slots.abort, &c->slots.whReq,
                                 UPD_TIMEOUT_MS);
        if (wok) {
            free(json); json = wjson; jlen = wlen; st = wst;
            wjson = NULL; ok = 1;
        } else if (wlen > jlen) {
            /* 两条链路都失败时，挑字节数多的一份作为原始响应展示 */
            free(json); json = wjson; jlen = wlen;
            if (!st) st = wst;
            wjson = NULL;
        } else {
            free(wjson); wjson = NULL;
        }
    }
    net_end(c);
    free(wjson);

    /* 看门狗已经提示过，这里不能再报一次 */
    if (!READ(&c->timedOut) && !READ(&g_shutdown)) {
        m = (UpdMsg *)calloc(1, sizeof(*m));
        if (m) {
            WCHAR info[UPD_TEXT_CHARS], wtag[64], wowner[128], wname[128];
            char tag[64] = { 0 };
            int maj = 0, min = 0, pat = 0, rev = 0;
            int cma = 0, cmi = 0, cpa = 0, cre = 0;

            wtag[0] = wowner[0] = wname[0] = 0;
            MultiByteToWideChar(CP_UTF8, 0, tag, -1, wtag, 64);
            MultiByteToWideChar(CP_UTF8, 0, QI_REPO_OWNER, -1, wowner, 128);
            MultiByteToWideChar(CP_UTF8, 0, QI_REPO_NAME, -1, wname, 128);

            if (!ok) {
                set_raw(m, L"API", st, json, jlen, api, we, ext);
            } else if (!json_str(json, "tag_name", tag, sizeof(tag)) ||
                       !parse_ver(tag, &maj, &min, &pat, &rev)) {
                set_raw(m, L"API（读不到版本号）", 200, json, jlen, api, 0, NULL);
            } else {
                
                tag_to_ui(tag, wtag, 64);
                parse_ver(QI_VERSION_STR, &cma, &cmi, &cpa, &cre);
                if (ver_cmp(maj, min, pat, rev, cma, cmi, cpa, cre) <= 0) {
                    _snwprintf(info, UPD_TEXT_CHARS,
                        L"已经是最新版了。\n\n当前版本：%s\n最新版本：%s",
                        QI_VERSION_UI_W, wtag);
                    info[UPD_TEXT_CHARS - 1] = 0;
                    set_info(m, UPD_KIND_LATEST, L"检查更新", info);
                } else {
                    _snwprintf(info, UPD_TEXT_CHARS,
                        L"发现新版本。\n\n当前版本：%s\n最新版本：%s\n\n"
                        L"点击「下载更新」获取安装包。",
                        QI_VERSION_UI_W, wtag);
                    info[UPD_TEXT_CHARS - 1] = 0;
                    set_info(m, UPD_KIND_NEWVER, L"发现新版本", info);
                    _snprintf(m->tag, sizeof(m->tag), "%s", tag);
                    m->tag[sizeof(m->tag) - 1] = 0;
                    (void)wowner; (void)wname;
                }
            }
            post_result(c->hwnd, m);
        }
    }
    free(json);

    EXCHANGE(&c->finished, 1);
    if (c->watch) {
        WaitForSingleObject(c->watch, 2000);
        CloseHandle(c->watch);
    }
    {
        int timedOut = READ(&c->timedOut);
        free(c);
        /* 超时时看门狗已经清过 busy； 这里绝不能再清一次，否则会把新一轮 检查的连点保护误清零 */
        if (!timedOut) InterlockedExchange(&g_busy, 0);
    }
    return 0;
}

/* 对外入口 */
void update_check(HWND hwnd)
{
    UpdCtx *c;
    HANDLE th;

    if (InterlockedCompareExchange(&g_busy, 1, 0) != 0) return;
    delete_raw_file();          /* 不留上一轮的诊断文件 */
    g_state = UPD_ST_CHECKING;

    /* 点下去立刻给右栏一个反馈，不等网络返回 */
    ZeroMemory(&g_result, sizeof(g_result));
    g_result.kind = UPD_KIND_NONE;
    wcscpy(g_result.text, L"正在检查更新…");
    g_scroll = 0.0;

    c = (UpdCtx *)calloc(1, sizeof(*c));
    if (!c) { InterlockedExchange(&g_busy, 0); return; }
    c->hwnd = hwnd;

    c->watch = CreateThread(NULL, 0, upd_watch, c, 0, NULL);
    th = CreateThread(NULL, 0, upd_thread, c, 0, NULL);
    if (!th) {
        DWORD e = GetLastError();
        WCHAR t[256];
        UpdMsg *m;
        EXCHANGE(&c->finished, 1);
        if (c->watch) { WaitForSingleObject(c->watch, 2000); CloseHandle(c->watch); }
        free(c);
        InterlockedExchange(&g_busy, 0);
        m = (UpdMsg *)calloc(1, sizeof(*m));
        if (m) {
            _snwprintf(t, 256, L"[阶段] 线程\n[Win32] %lu\n[响应] (无响应体)\n",
                       (unsigned long)e);
            set_info(m, UPD_KIND_ERROR, L"检查更新失败", t);
            post_result(hwnd, m);
        }
        return;
    }
    CloseHandle(th);
}

void update_on_message(App *a, WPARAM wp, LPARAM lp)
{
    UpdMsg *m;
    unsigned faIcon;
    DWORD ms = 8000;

    if (wp == 1) {
        UpdTimeoutMsg *tm = (UpdTimeoutMsg *)lp;
        int kind;
        if (!tm) return;
        m = (UpdMsg *)calloc(1, sizeof(*m));
        if (!m) { free(tm); return; }
        kind = tm->isDownload ? UPD_KIND_DLERROR : UPD_KIND_ERROR;
        set_info(m, kind, tm->isDownload ? L"下载超时" : L"检查更新失败", tm->msg);
        free(tm);
    } else {
        m = (UpdMsg *)lp;
        if (!m) return;
    }

    ZeroMemory(&g_result, sizeof(g_result));
    memcpy(&g_result, m, sizeof(*m));
    free(m);
    g_scroll = 0.0;

    switch (g_result.kind) {
    case UPD_KIND_LATEST:
        g_state = UPD_ST_LATEST; faIcon = FA_CHECK; ms = 8000; break;
    case UPD_KIND_NEWVER:
        g_state = UPD_ST_READY;
        _snprintf(g_newTag, sizeof(g_newTag), "%s", g_result.tag);
        g_newTag[sizeof(g_newTag) - 1] = 0;
        faIcon = FA_UP; ms = 12000; break;
    case UPD_KIND_DONE:
        g_state = UPD_ST_DONE;
        wcsncpy(g_setup, g_result.setup, 1023);
        g_setup[1023] = 0;
        faIcon = FA_CHECK; ms = 12000; break;
    case UPD_KIND_DLERROR:
        g_state = UPD_ST_READY; faIcon = FA_BELL; ms = 20000; break;
    default:
        g_state = UPD_ST_ERROR; faIcon = FA_BELL; ms = 20000; break;
    }

    /* 结果一律走 Qisland 自己的即时通知：内存态、不排队、到点自动消失， 不弹系统对话框、不阻塞主循环。 */
    if (!READ(&g_shutdown))
        notify_show_fa(&a->notify, a, faIcon,
                       g_result.title, g_result.brief, ms);
    a->dirty = 1;
}

void update_on_progress(App *a, WPARAM wp, LPARAM lp)
{
    if (g_state != UPD_ST_DOWNLOADING) return;
    g_permille = (int)wp;
    g_done = (DWORD)lp;
    a->dirty = 1;
}

int update_state(void) { return g_state; }
int update_progress_permille(void) { return g_permille; }
DWORD update_progress_bytes(void) { return g_done; }

/* tag_name 统一显示成 v 开头，下载直链仍用原始 tag */
static void tag_to_ui(const char *tag, WCHAR *out, int cap)
{
    WCHAR w[64];
    MultiByteToWideChar(CP_UTF8, 0, tag, -1, w, 64);
    if (w[0] == L'v' || w[0] == L'V') _snwprintf(out, cap, L"%s", w);
    else _snwprintf(out, cap, L"v%s", w);
    out[cap - 1] = 0;
}

/* tag_name 去掉前导 v 与非数字后缀，得到与 CI 产物名一致的版本号 */
static void tag_to_ver(const char *tag, char *ver, int cap)
{
    const char *p = tag;
    int i = 0;
    while (*p == 'v' || *p == 'V' || *p == ' ') p++;
    while (*p && i < cap - 1 && ((*p >= '0' && *p <= '9') || *p == '.'))
        ver[i++] = *p++;
    ver[i] = 0;
}

/* GitHub 的 /releases/download/TAG/ASSET 直链会 302 到对象存储，WinINet 自动跟随 */
static void make_setup_url(char *url, int cap, const char *tag, const char *ver)
{
    _snprintf(url, cap,
        "https://github.com/%s/%s/releases/download/%s/Qisland-%s-Setup.exe",
        QI_REPO_OWNER, QI_REPO_NAME, tag, ver);
    url[cap - 1] = 0;
}

/* 加速地址就是前缀加整条官方直链 */
static void make_proxy_url(char *out, int cap, const char *direct)
{
    _snprintf(out, cap, "%s%s", QI_GHPROXY_PREFIX, direct);
    out[cap - 1] = 0;
}

static void make_setup_path(WCHAR *dst, int cap, const char *ver)
{
    WCHAR dir[1024], wv[128];
    if (!GetTempPathW(1024, dir)) { dst[0] = 0; return; }
    MultiByteToWideChar(CP_UTF8, 0, ver, -1, wv, 128);
    _snwprintf(dst, cap, L"%sQisland-%s-Setup.exe", dir, wv);
    dst[cap - 1] = 0;
}

static void finish_ctx(UpdCtx *c)
{
    int timedOut;
    EXCHANGE(&c->finished, 1);
    if (c->watch) {
        WaitForSingleObject(c->watch, 2000);
        CloseHandle(c->watch);
    }
    timedOut = READ(&c->timedOut);
    free(c);
    /* 超时时看门狗已清过 busy，这里再清会把新一轮的连点保护误清零 */
    if (!timedOut) InterlockedExchange(&g_busy, 0);
}

/* 一条地址先走 WinINet，握手不上再走系统自带的 WinHTTP */
static int dl_once(UpdCtx *c, const char *url, DWORD *st, DWORD *wst,
                   char **body, int *blen, DWORD *we, WCHAR *ext, int extN)
{
    WCHAR wurl[2048];
    MultiByteToWideChar(CP_UTF8, 0, url, -1, wurl, 2048);
    *st = 0; *wst = 0; *we = 0;
    ext[0] = 0;
    if (qi_download_file(url, c->setup, st, body, blen, we, ext, extN,
                         &c->slots.inet, &c->slots.url,
                         &c->slots.abort, c->hwnd))
        return 1;
    if (READ(&c->slots.abort)) return 0;
    return qi_winhttp_download(wurl, c->setup, wst,
                               &c->slots.abort, &c->slots.whReq,
                               UPD_DL_RECV_MS, c->hwnd);
}

static DWORD WINAPI upd_dl_thread(LPVOID arg)
{
    UpdCtx *c = (UpdCtx *)arg;
    WCHAR ext[512] = L"";
    DWORD st = 0, wst = 0, we = 0;
    char *body = NULL;
    const char *used = c->url;
    int blen = 0, ok = 0, i;
    UpdMsg *m;

    net_begin(c, 2);
    /* 加速节点限流或返回落地页时，再用官方直链试一遍 */
    for (i = 0; i < 2; i++) {
        used = i ? c->direct : c->url;
        free(body);
        body = NULL;
        blen = 0;
        ok = dl_once(c, used, &st, &wst, &body, &blen, &we, ext, 512);
        if (ok || READ(&c->slots.abort)) break;
        PostMessageW(c->hwnd, WM_APP_UPDATE_PROGRESS, 0, 0);
    }
    net_end(c);

    if (!READ(&c->timedOut) && !READ(&g_shutdown)) {
        m = (UpdMsg *)calloc(1, sizeof(*m));
        if (m) {
            if (ok) {
                WCHAR info[UPD_TEXT_CHARS];
                _snwprintf(info, UPD_TEXT_CHARS,
                    L"下载完成。\n安装包：%s\n\n点击「退出Qisland并更新」开始安装。",
                    c->setup);
                info[UPD_TEXT_CHARS - 1] = 0;
                set_info(m, UPD_KIND_DONE, L"下载完成", info);
                wcsncpy(m->setup, c->setup, 1023);
                m->setup[1023] = 0;
            } else {
                if (!st) st = wst;
                set_raw(m, used == c->direct ? L"下载（官方直链）" : L"下载（加速）",
                        st, body, blen, used, we, ext);
                m->kind = UPD_KIND_DLERROR;
            }
            post_result(c->hwnd, m);
        }
    }
    free(body);
    finish_ctx(c);
    return 0;
}

static void update_start_download(HWND hwnd)
{
    UpdCtx *c;
    HANDLE th;
    char ver[64], direct[1024], proxy[1024];
    WCHAR dst[1024];

    if (InterlockedCompareExchange(&g_busy, 1, 0) != 0) return;
    tag_to_ver(g_newTag, ver, sizeof(ver));
    if (!ver[0] || !g_newTag[0]) { InterlockedExchange(&g_busy, 0); return; }
    make_setup_url(direct, sizeof(direct), g_newTag, ver);
    make_proxy_url(proxy, sizeof(proxy), direct);
    make_setup_path(dst, 1024, ver);
    if (!dst[0]) { InterlockedExchange(&g_busy, 0); return; }

    g_state = UPD_ST_DOWNLOADING;
    g_permille = 0;
    g_done = 0;
    ZeroMemory(&g_result, sizeof(g_result));
    g_result.kind = UPD_KIND_NONE;
    wcscpy(g_result.text, L"正在下载安装包…");
    g_scroll = 0.0;

    c = (UpdCtx *)calloc(1, sizeof(*c));
    if (!c) { InterlockedExchange(&g_busy, 0); return; }
    c->hwnd = hwnd;
    _snprintf(c->url, sizeof(c->url), "%s", proxy);
    c->url[sizeof(c->url) - 1] = 0;
    _snprintf(c->direct, sizeof(c->direct), "%s", direct);
    c->direct[sizeof(c->direct) - 1] = 0;
    wcsncpy(c->setup, dst, 1023);
    c->setup[1023] = 0;

    c->watch = CreateThread(NULL, 0, upd_watch, c, 0, NULL);
    th = CreateThread(NULL, 0, upd_dl_thread, c, 0, NULL);
    if (!th) {
        DWORD e = GetLastError();
        WCHAR t[256];
        UpdMsg *m;
        EXCHANGE(&c->finished, 1);
        if (c->watch) {
            WaitForSingleObject(c->watch, 2000);
            CloseHandle(c->watch);
        }
        free(c);
        InterlockedExchange(&g_busy, 0);
        g_state = UPD_ST_READY;
        m = (UpdMsg *)calloc(1, sizeof(*m));
        if (m) {
            _snwprintf(t, 256, L"[阶段] 线程\n[Win32] %lu\n[响应] (无响应体)\n",
                       (unsigned long)e);
            set_info(m, UPD_KIND_DLERROR, L"下载失败", t);
            post_result(hwnd, m);
        }
        return;
    }
    CloseHandle(th);
}

void update_button_click(HWND hwnd)
{
    switch (g_state) {
    case UPD_ST_CHECKING:
    case UPD_ST_DOWNLOADING:
        return;
    case UPD_ST_READY:
        update_start_download(hwnd);
        return;
    case UPD_ST_DONE:
        /* 安装包要查窗口类，先退出让窗口销毁，再在 cleanup 末尾运行 */
        if (!g_setup[0]) return;
        g_armed = 1;
        PostQuitMessage(0);
        return;
    default:
        update_check(hwnd);
        return;
    }
}

void update_launch_pending(void)
{
    HRESULT hr;
    HINSTANCE r;
    if (!g_armed || !g_setup[0]) return;
    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    r = ShellExecuteW(NULL, L"open", g_setup, NULL, NULL, SW_SHOWNORMAL);
    if (SUCCEEDED(hr)) CoUninitialize();
    if ((INT_PTR)r <= 32)
        MessageBoxW(NULL, L"无法启动安装包，请手动运行下载的文件。",
                    APP_NAME, MB_OK | MB_ICONERROR);
}

void update_shutdown(void)
{
    InterlockedExchange(&g_shutdown, 1);
}
