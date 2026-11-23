/*
 * Veranda Solitaire: where the stacks and cards are on the 640x480
 * screen, which card is under a point, and where a dragged card goes
 * (Game.getClosestStack with the layouts' regions). Portable: the host
 * tests use it to turn moves into mouse scripts.
 *
 * Copyright (C) 2026 the Veranda Solitaire authors. GPL-3.0-or-later
 * (see COPYING).
 */
#include "sol.h"
#include "layout.h"

u16 l_x[MAXSTACKS], l_y[MAXSTACKS], l_maxh[MAXSTACKS];
u8 l_fan[MAXSTACKS], l_show[MAXSTACKS];
static u8 region;               /* 0 none, 1 rows from l_rtop down, 2 rows left of l_rleft */
static u16 l_rtop, l_rleft;

static void at(u8 s, u16 x, u16 y, u8 fan)
{
    l_x[s] = x;
    l_y[s] = y;
    l_fan[s] = fan;
    l_show[s] = 1;
    l_maxh[s] = PLAY_BOTTOM - y;
}

void layout_init(void)
{
    u8 i, s;
    region = 0;
    switch (G->layout) {
    case L_KLONDIKE:
        at(st_talon, 48, TOP_Y, FAN_NONE);
        at(st_waste, 128, TOP_Y, FAN_RIGHT);
        l_show[st_waste] = G->num_deal;
        for (i = 0; i < 4; ++i)
            at(st_f0 + i, 288 + 80 * i, TOP_Y, FAN_NONE);
        for (i = 0; i < 7; ++i)
            at(st_r0 + i, 48 + 80 * i, ROW_Y, FAN_DOWN);
        region = 1;
        l_rtop = ROW_Y - CARD_H / 2;
        break;
    case L_FREECELL:
        for (i = 0; i < 4; ++i)
            at(st_v0 + i, 8 + 72 * i, TOP_Y, FAN_NONE);
        for (i = 0; i < 4; ++i)
            at(st_f0 + i, 360 + 72 * i, TOP_Y, FAN_NONE);
        for (i = 0; i < 8; ++i)
            at(st_r0 + i, 40 + 72 * i, ROW_Y, FAN_DOWN);
        at(st_talon, 0, 0, FAN_HIDDEN);
        region = 1;
        l_rtop = ROW_Y - CARD_H / 2;
        break;
    case L_SPIDER:
        at(st_talon, 8, TOP_Y, FAN_NONE);
        for (i = 0; i < 8; ++i)
            at(st_f0 + i, 136 + 64 * i, TOP_Y, FAN_NONE);
        for (i = 0; i < 10; ++i)
            at(st_r0 + i, 8 + 64 * i, ROW_Y, FAN_DOWN);
        region = 1;
        l_rtop = ROW_Y - CARD_H / 2;
        break;
    case L_GOLF:
        for (i = 0; i < 7; ++i)
            at(st_r0 + i, 48 + 80 * i, TOP_Y, FAN_DOWN);
        at(st_talon, 16, GOLF_Y, FAN_NONE);
        at(st_waste, 96, GOLF_Y, FAN_RIGHT);
        l_show[st_waste] = 20;
        break;
    case L_PYRAMID:
        s = st_r0;
        for (i = 0; i < 7; ++i) {
            u8 j;
            for (j = 0; j <= i; ++j)
                at(s++, 96 + (6 - i) * 32 + j * 64, TOP_Y + i * 40, FAN_NONE);
        }
        at(st_talon, 16, TOP_Y, FAN_NONE);
        at(st_waste, 16, TOP_Y + 96, FAN_NONE);
        at(st_f0, 568, TOP_Y, FAN_NONE);
        break;
    case L_YUKON:
        for (i = 0; i < 7; ++i)
            at(st_r0 + i, 16 + 72 * i, TOP_Y, FAN_DOWN);
        for (i = 0; i < 4; ++i)
            at(st_f0 + i, 560, TOP_Y + 96 * i, FAN_NONE);
        at(st_talon, 0, 0, FAN_HIDDEN);
        region = 2;
        l_rleft = 16 + 7 * 72 - CARD_W / 2;
        break;
    }
}

/* card i's top in column s with offsets od, ou: the face-up cards start
   on an 8 x 8 card of the screen (a card's colour is tiles of its own
   cards, draw.c), so after face-down cards at od = 4 the first face-up
   card's top is rounded up to a multiple of 8 (l_y is one) */
static u16 fan_y(u8 s, u8 i, u8 od, u8 ou)
{
    u8 k, n = s_n[s];
    u16 y = l_y[s];
    for (k = 0; k < i; ++k) {
        y += UP(s_c[s][k]) ? ou : od;
        if (!UP(s_c[s][k]) && k + 1 < n && UP(s_c[s][k + 1]))
            y = (y + 7) & ~7;
    }
    return y;
}

/* the offsets of a fanned column: face down od, face up ou, made smaller
   until the column fits in l_maxh (as PySolFC shrinks its stacks): the
   face-down cards' 8 to 4 first, then the face-up cards' 16 to 8 (only
   multiples of 8 for them: their colour stays on its cards) */
void fan_offsets(u8 s, u8 *pod, u8 *pou)
{
    u8 od = OFF_DOWN, ou = OFF_UP, n = s_n[s];
    for (;;) {
        if (!n || fan_y(s, n - 1, od, ou) - l_y[s] + CARD_H <= l_maxh[s])
            break;
        if (od > 4)
            od = 4;
        else if (ou > 8)
            ou = 8;
        else
            break;      /* (no smaller: the column ends below the table) */
    }
    *pod = od;
    *pou = ou;
}

/* card i's top left; FAN_RIGHT: the cards before the last l_show at the base */
void card_xy(u8 s, u8 i, u16 *px, u16 *py)
{
    u8 od, ou, k;
    u16 y = l_y[s];
    *px = l_x[s];
    switch (l_fan[s]) {
    case FAN_DOWN:
        fan_offsets(s, &od, &ou);
        y = fan_y(s, i, od, ou);
        break;
    case FAN_RIGHT:
        if (s_n[s] > l_show[s]) {
            k = s_n[s] - l_show[s];
            if (i > k)
                *px += (u16)(i - k) * FAN_DX;
        } else {
            *px += (u16)i * FAN_DX;
        }
        break;
    }
    *py = y;
}

/* the area a stack covers now: [x0, x1) x [y0, y1) */
void stack_rect(u8 s, u16 *x0, u16 *y0, u16 *x1, u16 *y1)
{
    u16 x, y;
    *x0 = l_x[s];
    *y0 = l_y[s];
    if (s_n[s])
        card_xy(s, s_n[s] - 1, &x, &y);
    else {
        x = l_x[s];
        y = l_y[s];
    }
    *x1 = x + CARD_W;
    *y1 = y + CARD_H;
}

/* the stack and card under a point (the topmost: stacks drawn later lie
   over earlier ones); *pi = ANY: the stack's empty place */
u8 hit_test(u16 px, u16 py, u8 *pi)
{
    u8 s, i;
    u16 x, y;
    for (s = nstacks; s-- > 0;) {
        if (l_fan[s] == FAN_HIDDEN)
            continue;
        if (!s_n[s]) {
            if (px >= l_x[s] && px < l_x[s] + CARD_W && py >= l_y[s] && py < l_y[s] + CARD_H) {
                *pi = ANY;
                return s;
            }
            continue;
        }
        for (i = s_n[s]; i-- > 0;) {
            card_xy(s, i, &x, &y);
            if (px >= x && px < x + CARD_W && py >= y && py < y + CARD_H) {
                *pi = i;
                return s;
            }
            if (l_fan[s] == FAN_NONE)
                break;
        }
    }
    return ANY;
}

/* Game.getClosestStack: within the region the card's corner is in, the
   open stack whose place is nearest */
u8 closest_stack(u16 cx, u16 cy)
{
    u8 s, best = ANY, rows, isrow;
    u16 d, bd = 0xFFFF;
    int dx, dy;
    rows = 0;
    if (region == 1 && cy >= l_rtop)
        rows = 1;
    if (region == 2 && cx < l_rleft)
        rows = 1;
    for (s = 0; s < nstacks; ++s) {
        if (l_fan[s] == FAN_HIDDEN || !sol_stack_is_open(s))
            continue;
        isrow = (u8)(s >= st_r0 && s < st_r0 + n_rows);
        if (region && isrow != rows)
            continue;
        dx = ((int)l_x[s] - (int)cx) / 4;
        dy = ((int)l_y[s] - (int)cy) / 4;
        d = (u16)(dx * dx) + (u16)(dy * dy);
        if (d < bd) {
            bd = d;
            best = s;
        }
    }
    return best;
}
