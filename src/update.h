/* update.h — 设置页「更新」：查询最新 Release，把结果交回主线程
 *
 * 网络 IO 全在工作线程，主线程只负责把结果放进右栏可滚动文本区，
 * 并用 Qisland 自己的即时通知报一条。任何失败都保留服务器返回的原始
 * 字节，完整副本另存到 %TEMP%\Qisland_update_error.txt。
 */
#ifndef QISLAND_UPDATE_H
#define QISLAND_UPDATE_H

#include "common.h"

/* 工作线程 / 看门狗 → 主线程。wp=0 带 UpdMsg*，wp=1 带 UpdTimeoutMsg*。
   占用 WM_APP+4（+1 单实例 / +2 气泡 / +3 托盘已用）。 */
#define WM_APP_UPDATE       (WM_APP + 4)

#define UPD_BRIEF_CHARS     256     /* 通知正文上限，与 NOTIFY_DETAIL_MAX 对齐 */
#define UPD_TEXT_CHARS      8192    /* 右栏结果区上限 */
#define UPD_RAW_SHOW        2000    /* 原始响应最多贴多少字符 */
#define UPD_RAW_FILE        L"Qisland_update_error.txt"

#define UPD_KIND_NONE       0
#define UPD_KIND_LATEST     1
#define UPD_KIND_NEWVER     2
#define UPD_KIND_ERROR      3

typedef struct {
    int     kind;
    DWORD   status;
    WCHAR   title[64];
    WCHAR   brief[UPD_BRIEF_CHARS];
    WCHAR   text[UPD_TEXT_CHARS];
} UpdMsg;

/* 超时消息不引用任何检查上下文，工作线程可能永远醒不过来 */
typedef struct {
    WCHAR   msg[300];
} UpdTimeoutMsg;

void update_check(HWND hwnd);                       /* 点「检查更新」 */
void update_on_message(App *a, WPARAM wp, LPARAM lp);  /* 主线程：通知 + 结果区 */
void update_shutdown(void);
int  update_busy(void);

/* 右栏结果区：只有主线程读写 */
int          update_result_kind(void);
const WCHAR *update_result_text(void);
double       update_result_scroll(void);
void         update_result_set_doch(double h, double viewH);
void         update_result_wheel(int dir, double viewH);
void         update_result_reset_scroll(void);

/* WinHTTP 兜底放在独立翻译单元：wininet.h 与 winhttp.h 的 typedef 冲突，
   两边不能出现在同一个编译单元里。句柄槽统一用 PVOID / LONG 传递。 */
int  qi_winhttp_get(const WCHAR *url, const WCHAR *headers,
                    char **out, int *plen, DWORD *status,
                    volatile LONG *abort, volatile PVOID *reqSlot,
                    DWORD tmo);
void qi_winhttp_abort(volatile PVOID *reqSlot);

#endif /* QISLAND_UPDATE_H */
