/* canvas.h — 逐像素预乘 BGRA 画布（自写光栅化，不依赖第三方库） */
#ifndef QISLAND_CANVAS_H
#define QISLAND_CANVAS_H

#include "common.h"

typedef struct {
    int    W, H;
    double u;
    int    ss;
    /* 全局裁剪矩形：所有覆盖比叠加都会受它约束。
       便签文本用它卡在内容区里，上下超出的行不会画到卡片外。 */
    int    clipX0, clipY0, clipX1, clipY1;
    HDC    outDC, txtDC;
    HBITMAP outBmp, txtBmp;
    void  *outBits, *txtBits;      /* 32bpp top-down DIB */
} Canvas;

int    canvas_init(Canvas *c, int W, int H, double u, int ss);
void   canvas_shutdown(Canvas *c);
void   canvas_set_clip(Canvas *c, int x0, int y0, int x1, int y1);
void   canvas_reset_clip(Canvas *c);

/* 主形状：圆角矩形 SDF + 过渡带面积覆盖率超采样 */
void   canvas_shape(Canvas *c, double w, double h, double r);
/* 覆盖比叠加原语（设备像素坐标） */
void   canvas_blend_px(Canvas *c, int x, int y,
                       int cr, int cg, int cb, double k);
/* 以下三个会**写入 alpha 通道**，允许在形状外的透明区域新建浮层。
   blend_px 只在 da>0 的像素上叠色（控件用，天然被圆角裁切）；
   write_px 走完整 src-over，用于 tooltip 这类要浮出卡片的临时气泡。 */
void   canvas_write_px(Canvas *c, int x, int y,
                       int cr, int cg, int cb, double a);
void   canvas_round_alpha(Canvas *c, double x, double y, double w, double h,
                          double r, int cr, int cg, int cb, double a);
void   canvas_tri(Canvas *c, double cx, double yTop, double halfW, double h,
                  int dir, int cr, int cg, int cb, double a);
void   canvas_round(Canvas *c, double x, double y, double w, double h,
                    double r, int cr, int cg, int cb, double k);
void   canvas_frame(Canvas *c, double x, double y, double w, double h,
                    double r, double th, int cr, int cg, int cb, double k);
void   canvas_disc(Canvas *c, double cx, double cy, double rad,
                   int cr, int cg, int cb, double k);
void   canvas_line(Canvas *c, double ax, double ay, double bx, double by,
                   double th, int cr, int cg, int cb, double k);

int    canvas_alpha_at(const Canvas *c, int x, int y);
void   canvas_present(Canvas *c, HWND hwnd);
void   canvas_dump_bmp(Canvas *c, const char *path);

/* SDF 供命中判定复用（设备像素坐标） */
double canvas_sd_roundbox(double px, double py, double cx, double cy,
                          double hw, double hh, double r);

#endif /* QISLAND_CANVAS_H */
