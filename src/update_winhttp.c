/* update_winhttp.c — WinHTTP GET 兜底 */
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "update.h"          /* common.h 提供 windows.h 与 WINVER */
#include <winhttp.h>
#include <stdlib.h>
#include <string.h>

#define WH_MAX_BODY   (1024 * 1024)   /* API 是 JSON，1 MB 足够 */
#define WH_READ(p)    InterlockedCompareExchange((volatile LONG *)(p), 0, 0)

/* 成功返回 1； 失败返回 0，但 *out 仍可能是已经读到的原始响应体，由调用方 决定要不要展示。 */
int qi_winhttp_get(const WCHAR *url, const WCHAR *headers,
                   char **out, int *plen, DWORD *status,
                   volatile LONG *abort, volatile PVOID *reqSlot,
                   DWORD tmo)
{
    URL_COMPONENTSW uc;
    WCHAR host[256] = L"", path[2048] = L"";
    HINTERNET hS = NULL, hC = NULL, hR = NULL;
    DWORD code = 0, sz, proto, rp, ct, rt;
    char *buf = NULL;
    size_t cap = 16384, len = 0;
    int ok = 0, secure, failed;

    *out = NULL; *plen = 0;
    if (status) *status = 0;

    ZeroMemory(&uc, sizeof(uc));
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = host; uc.dwHostNameLength = 256;
    uc.lpszUrlPath  = path; uc.dwUrlPathLength  = 2048;
    if (!WinHttpCrackUrl(url, 0, 0, &uc)) return 0;
    secure = (uc.nScheme == INTERNET_SCHEME_HTTPS);

    /* Win7 没有 Win8 才有的 AUTOMATIC_PROXY，只能用 DEFAULT_PROXY */
    hS = WinHttpOpen(L"Qisland", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hS) return 0;
    ct = tmo; rt = tmo;
    WinHttpSetOption(hS, WINHTTP_OPTION_CONNECT_TIMEOUT, &ct, sizeof(ct));
    WinHttpSetOption(hS, WINHTTP_OPTION_SEND_TIMEOUT, &rt, sizeof(rt));
    WinHttpSetOption(hS, WINHTTP_OPTION_RECEIVE_TIMEOUT, &rt, sizeof(rt));
    WinHttpSetOption(hS, WINHTTP_OPTION_RECEIVE_RESPONSE_TIMEOUT, &rt, sizeof(rt));

    hC = WinHttpConnect(hS, host,
                        uc.nPort ? uc.nPort
                                 : (INTERNET_PORT)(secure ? INTERNET_DEFAULT_HTTPS_PORT
                                                          : INTERNET_DEFAULT_HTTP_PORT),
                        0);
    if (!hC) goto done;
    hR = WinHttpOpenRequest(hC, L"GET", path[0] ? path : L"/", NULL,
                            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                            secure ? WINHTTP_FLAG_SECURE : 0);
    if (!hR) goto done;
    if (reqSlot) InterlockedExchangePointer(reqSlot, hR);

    if (secure) {
        /* Win7 SP1 需 KB3140245 才真正握得上手； 没打补丁时 SetOption 可能 失败，忽略即可，不能因为一个 option 就放弃这次请求。 */
        proto = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2
              | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1
              | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1
              | WINHTTP_FLAG_SECURE_PROTOCOL_SSL3;
        WinHttpSetOption(hR, WINHTTP_OPTION_SECURE_PROTOCOLS, &proto, sizeof(proto));
    }
    rp = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(hR, WINHTTP_OPTION_REDIRECT_POLICY, &rp, sizeof(rp));

    if (!WinHttpSendRequest(hR, headers, 0, WINHTTP_NO_REQUEST_DATA,
                            0, 0, 0) ||
        !WinHttpReceiveResponse(hR, NULL)) goto done;

    sz = sizeof(code);
    if (WinHttpQueryHeaders(hR,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &code, &sz,
            WINHTTP_NO_HEADER_INDEX))
        if (status) *status = code;

    buf = (char *)malloc(cap + 1);
    if (!buf) goto done;
    ok = 1;
    for (;;) {
        DWORD avail = 0, got = 0;
        if (abort && WH_READ(abort)) { ok = 0; break; }
        if (!WinHttpQueryDataAvailable(hR, &avail)) { ok = 0; break; }
        if (avail == 0) break;
        if (len + avail + 1 > cap) {
            size_t nc = cap * 2;
            char *nb;
            if (nc > WH_MAX_BODY) nc = WH_MAX_BODY;
            if (nc <= len + avail + 1) { ok = 0; break; }
            nb = (char *)realloc(buf, nc + 1);
            if (!nb) { ok = 0; break; }
            buf = nb; cap = nc;
        }
        if (!WinHttpReadData(hR, buf + len, avail, &got)) { ok = 0; break; }
        len += got;
        if (len >= WH_MAX_BODY) { ok = 0; break; }
        if (got == 0) break;
    }
    buf[len] = 0;

    failed = (!ok || code >= 400 || len == 0);
    if (failed) {
        /* 失败也把读到的原始响应交给调用方，别在这儿丢掉 */
        if (len > 0) { *out = buf; *plen = (int)len; } else free(buf);
        buf = NULL;
        ok = 0;
    } else {
        *out = buf; *plen = (int)len;
        buf = NULL;
        ok = 1;
    }

done:
    /* 取回句柄所有权再关：看门狗可能已经取走并关闭了 */
    if (reqSlot) {
        HINTERNET mine = (HINTERNET)InterlockedExchangePointer(reqSlot, NULL);
        if (mine) WinHttpCloseHandle(mine);
        hR = NULL;
    }
    if (hR) WinHttpCloseHandle(hR);
    if (hC) WinHttpCloseHandle(hC);
    if (hS) WinHttpCloseHandle(hS);
    if (buf) free(buf);
    return ok;
}

void qi_winhttp_abort(volatile PVOID *reqSlot)
{
    if (!reqSlot) return;
    HINTERNET h = (HINTERNET)InterlockedExchangePointer(reqSlot, NULL);
    if (h) WinHttpCloseHandle(h);
}
