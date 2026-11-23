/*
 * Veranda Solitaire: the screen layout (layout.c).
 * GPL-3.0-or-later (see COPYING).
 */
#ifndef LAYOUT_H
#define LAYOUT_H

#define CARD_W      56
#define CARD_H      80
#define CARD_B      7           /* bytes across */
#define MENU_H      17          /* the menu bar (Veranda Look 2: the kernel's,
                                   16 lines and a line under it) */
/* the rows of cards start on 8 x 8 cards (the colour map's), and a fan's
   offsets are 8 and 16, so a card picture fills whole cards of it from
   its top to its bottom: the felt's colour stops at its edges (draw.c,
   felt; but for a column squeezed to fit, fan_offsets) */
#define TOP_Y       24
#define ROW_Y       120
#define GOLF_Y      320
#define PLAY_TOP    17
#define PLAY_BOTTOM 464         /* the status bar below: card rows 58-59 */
#define STATUS_Y    464         /* the status bar's line (its first row) */
#define OFF_DOWN    8
#define OFF_UP      16          /* (was 18: on cards too, so a column ends on
                                   one; its corner index is rows 4-14) */
#define FAN_DX      24

enum { FAN_NONE, FAN_DOWN, FAN_RIGHT, FAN_HIDDEN };

extern u16 l_x[MAXSTACKS], l_y[MAXSTACKS], l_maxh[MAXSTACKS];
extern u8 l_fan[MAXSTACKS], l_show[MAXSTACKS];

void layout_init(void);
void fan_offsets(u8 s, u8 *od, u8 *ou);
void card_xy(u8 s, u8 i, u16 *x, u16 *y);
void stack_rect(u8 s, u16 *x0, u16 *y0, u16 *x1, u16 *y1);
u8 hit_test(u16 px, u16 py, u8 *pi);
u8 closest_stack(u16 cx, u16 cy);

#endif
