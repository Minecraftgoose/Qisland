/* update_dl.c — 流式下载 Release 安装包，按字节数回报进度 */
#include "update.h"
#include "config.h"
#include "version.h"

#include <wininet.h>
#include <stdio.h>

#define DL_BUF          65536
#define DL_MIN_SETUP    65536
#define DL_UA           L"User-Agent: Qisland/" QI_VERSION_W L"\r\n"
#define DL_ABORT(p)     InterlockedCompareExchange((volatile LONG *)(p), 0, 0)

/* 句柄所有权交给 slot：看门狗超时时会取走并关闭同一批句柄 */
static void close_slots(volatile PVOID *inetSlot, volatile PVOID *urlSlot,
                        HINTERNET hInet, HINTERNET hUrl)
{
    HINTERNET i = inetSlot ? (HINTERNET)InterlockedExchangePointer(inetSlot, NULL) : hInet;
    HINTERNET u = urlSlot  ? (HINTERNET)InterlockedExchangePointer(urlSlot, NULL)  : hUrl;
    if (u) InternetCloseHandle(u);
    if (i) InternetCloseHandle(i);
}

/* 4xx/5xx 时把错误页读回来，交给结果区原样显示 */
static void grab_error(HINTERNET hUrl, char **errBody, int *errLen)
{
    char *b;
    DWORD cap = 16384, len = 0, got = 0;
    if (!errBody) return;
    b = (char *)malloc(cap + 1);
    if (!b) return;
    while (len + 4096 < cap && len < UPD_DL_MAX_BODY) {
        got = 0;
        if (!InternetReadFile(hUrl, b + len, (DWORD)(cap - len - 1), &got) || got == 0) break;
        len += got;
    }
    b[len] = 0;
    *errBody = b;
    if (errLen) *errLen = (int)len;
}

/* 下下来的必须是 PE，403/404 网页不能当安装包运行 */
static int is_pe(const WCHAR *dst)
{
    HANDLE h = CreateFileW(dst, GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    LARGE_INTEGER sz;
    unsigned char mz[2] = { 0 };
    DWORD got = 0;
    int ok;
    if (h == INVALID_HANDLE_VALUE) return 0;
    sz.QuadPart = 0;
    GetFileSizeEx(h, &sz);
    ReadFile(h, mz, 2, &got, NULL);
    CloseHandle(h);
    ok = (sz.QuadPart >= DL_MIN_SETUP && got == 2 && mz[0] == 'M' && mz[1] == 'Z');
    return ok;
}

int qi_download_file(const char *url, const WCHAR *dst, DWORD *status,
                     char **errBody, int *errLen, DWORD *winerr,
                     WCHAR *winext, int winextN,
                     volatile PVOID *inetSlot, volatile PVOID *urlSlot,
                     volatile LONG *abort, HWND hwnd)
{
    HINTERNET hInet = NULL, hUrl = NULL;
    WCHAR wurl[2048];
    DWORD flags, ct, tmo, code = 0, sz, total = 0, done = 0, got = 0, wr = 0;
    HANDLE hf = INVALID_HANDLE_VALUE;
    char *buf = NULL;
    int ok = 0;

    if (errBody) *errBody = NULL;
    if (errLen) *errLen = 0;
    if (status) *status = 0;
    if (winerr) *winerr = 0;
    if (winext && winextN) winext[0] = 0;
    DeleteFileW(dst);

    hInet = InternetOpenW(L"Qisland/" QI_VERSION_W,
                          INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInet) { if (winerr) *winerr = GetLastError(); return 0; }
    if (inetSlot) InterlockedExchangePointer(inetSlot, hInet);

    ct  = UPD_DL_CONNECT_MS;
    tmo = UPD_DL_RECV_MS;
    InternetSetOption(hInet, INTERNET_OPTION_CONNECT_TIMEOUT,      &ct,  sizeof(ct));
    InternetSetOption(hInet, INTERNET_OPTION_SEND_TIMEOUT,         &tmo, sizeof(tmo));
    InternetSetOption(hInet, INTERNET_OPTION_RECEIVE_TIMEOUT,      &tmo, sizeof(tmo));
    InternetSetOption(hInet, INTERNET_OPTION_DATA_RECEIVE_TIMEOUT, &tmo, sizeof(tmo));

    if (MultiByteToWideChar(CP_UTF8, 0, url, -1, wurl, 2048) <= 0) goto done;
    flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE |
            INTERNET_FLAG_NO_UI | INTERNET_FLAG_PRAGMA_NOCACHE |
            INTERNET_FLAG_NO_COOKIES;
    if (strncmp(url, "https://", 8) == 0) flags |= INTERNET_FLAG_SECURE;
    hUrl = InternetOpenUrlW(hInet, wurl, DL_UA, 0, flags, 0);
    if (!hUrl) {
        if (winerr) *winerr = GetLastError();
        if (winext && winextN) {
            DWORD e = 0, n = (DWORD)winextN;
            InternetGetLastResponseInfoW(&e, winext, &n);
        }
        goto done;
    }
    if (urlSlot) InterlockedExchangePointer(urlSlot, hUrl);
    InternetSetOption(hUrl, INTERNET_OPTION_RECEIVE_TIMEOUT,      &tmo, sizeof(tmo));
    InternetSetOption(hUrl, INTERNET_OPTION_DATA_RECEIVE_TIMEOUT, &tmo, sizeof(tmo));

    sz = sizeof(code);
    HttpQueryInfoW(hUrl, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                   &code, &sz, NULL);
    if (status) *status = code;
    if (code >= 400) {
        grab_error(hUrl, errBody, errLen);
        if (winext && winextN) {
            DWORD e = 0, n = (DWORD)winextN;
            InternetGetLastResponseInfoW(&e, winext, &n);
        }
        goto done;
    }

    /* Content-Length 给进度条当分母；拿不到就只显示已下载字节数 */
    sz = sizeof(total);
    HttpQueryInfoW(hUrl, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER,
                   &total, &sz, NULL);

    hf = CreateFileW(dst, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                     FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf == INVALID_HANDLE_VALUE) {
        if (winerr) *winerr = GetLastError();
        goto done;
    }
    buf = (char *)malloc(DL_BUF);
    if (!buf) goto done;

    for (;;) {
        if (abort && DL_ABORT(abort)) goto done;
        got = 0;
        if (!InternetReadFile(hUrl, buf, DL_BUF, &got)) {
            if (winerr && !*winerr) *winerr = GetLastError();
            goto done;
        }
        if (got == 0) break;
        wr = 0;
        if (!WriteFile(hf, buf, got, &wr, NULL) || wr != got) goto done;
        done += got;
        if (done > UPD_DL_MAX_SETUP) goto done;
        PostMessageW(hwnd, WM_APP_UPDATE_PROGRESS,
                     total ? (WPARAM)(DWORD)(((DWORDLONG)done * 1000) / total) : 0,
                     (LPARAM)done);
    }
    CloseHandle(hf);
    hf = INVALID_HANDLE_VALUE;
    ok = (done >= DL_MIN_SETUP && is_pe(dst));

done:
    if (hf != INVALID_HANDLE_VALUE) CloseHandle(hf);
    if (buf) free(buf);
    close_slots(inetSlot, urlSlot, hInet, hUrl);
    if (!ok) {
        DeleteFileW(dst);
        if (winerr && !*winerr) *winerr = GetLastError();
    }
    return ok;
}
