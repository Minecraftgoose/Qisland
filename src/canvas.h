/* canvas.h — 预乘 BGRA 画布 */
#ifndef QISLAND_CANVAS_H
#define QISLAND_CANVAS_H

#include "common.h"

typedef struct {
    int    W, H;
    double u;
    int    ss;
    int    clipX0, clipY0, clipX1, clipY1;
    HDC    outDC, txtDC;
    HBITMAP outBmp, txtBmp;
    void  *outBits, *txtBits;      /* 32bpp top-down DIB */
} Canvas;

int    canvas_init(Canvas *c, int W, int H, double u, int ss);
void   canvas_shutdown(Canvas *c);
void   canvas_set_clip(Canvas *c, int x0, int y0, int x1, int y1);
void   canvas_reset_clip(Canvas *c);

void   canvas_shape(Canvas *c, double w, double h, double r);
void   canvas_blend_px(Canvas *c, int x, int y,
                       int cr, int cg, int cb, double k);

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

double canvas_sd_roundbox(double px, double py, double cx, double cy,
                          double hw, double hh, double r);

#endif /* QISLAND_CANVAS_H */
