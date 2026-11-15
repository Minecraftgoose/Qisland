/* icons.h — Font Awesome 6.7.2 Free Solid 图标码点
 *
 * 全部码点取自官方 npm 包内的 metadata/icons.yml（字体同包 webfonts/fa-solid-900.ttf）。
 * 官方文档：https://fontawesome.com/v6/docs/web/setup/host-yourself/webfonts
 *   Solid Free 对应文件名 fa-solid-900.ttf，CSS 家族 'Font Awesome 6 Free'。
 * 字体内部 nameID=1/4 为 "Font Awesome 6 Free Solid"（GDI 用这个，见 font.c）。
 *
 * 许可：图标 CC BY 4.0 / 字体 SIL OFL 1.1 / 元数据 MIT，见 assets/LICENSE.txt。
 *
 * 本文件由 metadata 生成，勿手改；新增图标请重新查官方 icons.yml 的 unicode 字段。
 */
#ifndef QISLAND_ICONS_H
#define QISLAND_ICONS_H

#define FA_HIDE             0xF070  /* eye-slash            */
#define FA_HIDE_ON          0xF06E  /* eye                  */
#define FA_VOLUME           0xF028  /* volume-high          */
#define FA_VOLUME_LOW       0xF026  /* volume-off           */
#define FA_VOLUME_MUTE      0xF6A9  /* volume-xmark         */
#define FA_BRIGHTNESS       0xF185  /* sun                  */
#define FA_BRIGHTNESS_LOW   0xF186  /* moon                 */
#define FA_TASKMGR          0xF0AE  /* list-check           */
#define FA_SLIDERS          0xF1DE  /* sliders              */
#define FA_PLAY             0xF04B  /* play                 */
#define FA_PAUSE            0xF04C  /* pause                */
#define FA_RESET            0xF1DA  /* clock-rotate-left    */
#define FA_CLOCK            0xF017  /* clock                */
#define FA_UP               0xF077  /* chevron-up           */
#define FA_DOWN             0xF078  /* chevron-down         */
#define FA_STOP             0xF04D  /* stop                 */
#define FA_GEAR             0xF013  /* gear                 */
#define FA_BELL             0xF0F3  /* bell（倒计时结束提示）*/
#define FA_PENCIL           0xF303  /* pencil（便签空态，无文字提示） */
#define FA_CLIPBOARD        0xF328  /* clipboard（历史空态，无文字提示） */
#define FA_TRASH_CAN        0xF2ED  /* trash-can（清空历史，仅图标） */
#define FA_CHECK            0xF00C  /* check（该条已回填，可直接粘贴） */
#define FA_XMARK            0xF00D  /* xmark（通知卡片右上角关闭，仅图标） */
/* 消息通知兜底图标：FA Free Solid 不含 weixin/qq 品牌字形，
   优先用 notify_icon_from_file() 取软件自身 HICON，取不到才用这两个。 */
#define FA_COMMENT_DOTS     0xF4AD  /* comment-dots（微信兜底） */
#define FA_COMMENT          0xF075  /* comment（QQ 兜底） */

#endif /* QISLAND_ICONS_H */
