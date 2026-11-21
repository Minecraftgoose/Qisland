/* update.h — 设置页「更新」：查 Release、下载安装包、退出并运行 */
#ifndef QISLAND_UPDATE_H
#define QISLAND_UPDATE_H

#include "common.h"

/* 工作线程 / 看门狗 → 主线程。 wp=0 带 UpdMsg*，wp=1 带 UpdTimeoutMsg*。 */
#define WM_APP_UPDATE           (WM_APP + 4)
#define WM_APP_UPDATE_PROGRESS  (WM_APP + 5)

#define UPD_BRIEF_CHARS     256     /* 通知正文上限，与 NOTIFY_DETAIL_MAX 对齐 */
#define UPD_TEXT_CHARS      8192    /* 右栏结果区上限 */
#define UPD_RAW_SHOW        2000    /* 原始响应最多贴多少字符 */
#define UPD_RAW_FILE        L"Qisland_update_error.txt"

#define UPD_DL_HARD_MS      330000  /* 下载 15 MB 安装包的看门狗阈值 */
#define UPD_DL_CONNECT_MS   20000
#define UPD_DL_RECV_MS      60000
#define UPD_DL_MAX_SETUP    (64 * 1024 * 1024)
#define UPD_DL_MAX_BODY     (64 * 1024)

#define UPD_KIND_NONE       0
#define UPD_KIND_LATEST     1
#define UPD_KIND_NEWVER     2
#define UPD_KIND_ERROR      3
#define UPD_KIND_DONE       4
#define UPD_KIND_DLERROR    5

/* 按钮状态：主线程独占写，绘制与点击分派都读这一份 */
#define UPD_ST_IDLE         0
#define UPD_ST_CHECKING     1
#define UPD_ST_READY        2
#define UPD_ST_DOWNLOADING  3
#define UPD_ST_DONE         4
#define UPD_ST_LATEST       5
#define UPD_ST_ERROR        6

typedef struct {
    int     kind;
    DWORD   status;
    WCHAR   title[64];
    WCHAR   brief[UPD_BRIEF_CHARS];
    WCHAR   text[UPD_TEXT_CHARS];
    char    tag[64];          /* 发现新版本时的 tag_name，供构造下载地址 */
    WCHAR   setup[1024];      /* 下载完成后的安装包绝对路径 */
} UpdMsg;

/* 超时消息不引用任何检查上下文，工作线程可能永远醒不过来 */
typedef struct {
    WCHAR   msg[300];
    int     isDownload;
} UpdTimeoutMsg;

void update_check(HWND hwnd);                       /* 点「检查更新」 */
void update_button_click(HWND hwnd);                /* 按当前状态分派按钮动作 */
void update_on_message(App *a, WPARAM wp, LPARAM lp);  /* 主线程：通知 + 结果区 */
void update_on_progress(App *a, WPARAM wp, LPARAM lp);
void update_launch_pending(void);                   /* 窗口销毁后运行安装包 */
void update_shutdown(void);
int  update_busy(void);

/* 右栏结果区：只有主线程读写 */
int          update_result_kind(void);
const WCHAR *update_result_text(void);
double       update_result_scroll(void);
void         update_result_set_doch(double h, double viewH);
void         update_result_wheel(int dir, double viewH);
void         update_result_reset_scroll(void);

int  update_state(void);
int  update_progress_permille(void);
DWORD update_progress_bytes(void);

/* WinINet 流式下载：进度直接 PostMessage 回主线程 */
int  qi_download_file(const char *url, const WCHAR *dst, DWORD *status,
                      char **errBody, int *errLen, DWORD *winerr,
                      WCHAR *winext, int winextN,
                      volatile PVOID *inetSlot, volatile PVOID *urlSlot,
                      volatile LONG *abort, HWND hwnd);

/* WinHTTP 兜底放在独立翻译单元：wininet.h 与 winhttp.h 的 typedef 冲突 */
int  qi_winhttp_get(const WCHAR *url, const WCHAR *headers,
                    char **out, int *plen, DWORD *status,
                    volatile LONG *abort, volatile PVOID *reqSlot,
                    DWORD tmo);
int  qi_winhttp_download(const WCHAR *url, const WCHAR *dst, DWORD *status,
                         volatile LONG *abort, volatile PVOID *reqSlot,
                         DWORD tmo, HWND hwnd);
void qi_winhttp_abort(volatile PVOID *reqSlot);

#endif /* QISLAND_UPDATE_H */
