/* about.h — 设置页「关于」右栏（软件图标 + 名称 + 仓库链接 + 四个胶囊板块）
 *
 * 用户先看 HTML 效果稿确认，再落进 C。文案逐字来自确认稿，登记在
 * AGENTS.md 第 4 节封闭清单第 15 条 —— **不是解除文字政策**。
 *
 * 0.3 轮：字号整体放大，四个板块与板块内胶囊全部单列纵向排布；
 * 内容远高于可视区，右栏可滚轮滚动。文档高度、裁剪框、滚动条全部由
 * settings_boxes() 给的 aboutBody 推导，绘制与滚轮判定同源。
 * 仓库地址是整行可点的链接：命中框取 SetBoxes.aboutRepo，点击由
 * ui.c 的 H_ABOUT_REPO → sys_open_repo() 打开系统默认浏览器。
 */
#ifndef QISLAND_ABOUT_H
#define QISLAND_ABOUT_H

#include "common.h"
#include "settings.h"

/* 由 settings_draw() 在 tab == SET_TAB_ABOUT 时调用。
   W/H/L 是目标态标称尺寸（设备像素），k 是展开淡入系数。 */
void about_draw(App *a, double W, double H, double L,
                const SetBoxes *b, double k);

/* dir > 0 = 往下滚（内容上移）。viewH 取 aboutBody.h。
   只改滚动偏移，不置脏；调用方在真正变化时置一次脏。 */
void about_wheel(App *a, double viewH, int dir);

#endif /* QISLAND_ABOUT_H */
