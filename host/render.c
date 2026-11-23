/*
 * Veranda Solitaire: the table drawn on the host from a state, whole and
 * from scratch (every card in full, stacks in order), with the program's
 * pictures and layout: what the X16's screen must show, however it got
 * there by redrawing only what changed; and its colour (Look 3: the tile
 * map's entries, the cards' maps of tools/mkcards.py, a corner's tile as
 * draw.c chooses it by what lies under it). GPL-3.0-or-later (see
 * COPYING).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "host.h"
#include "cards.h"

static u8 pics[PICS][PIC_REC];

/* the tile map's entries of the screen's 8 x 8 cards (80 a row, 60 rows;
   the table's rows 2-57): what tools/check_look.py finds in VRAM */
unsigned short cmap[60 * 80];
static const u8 mtop0[] = MASK_TOP0, mbot0[] = MASK_BOT0, mtop6[] = MASK_TOP6, mbot6[] = MASK_BOT6;

int load_pics(const char *dir)
{
    char p[400];
    int r, k, n = 0;
    FILE *f;
    for (r = 1; r <= PIC_RECORDS; ++r) {
        sprintf(p, "%s/cards.%d", dir, r);
        f = fopen(p, "rb");
        if (!f)
            return 0;
        for (k = 0; k < PIC_PER_RECORD && n < PICS; ++k, ++n)
            if (fread(pics[n], PIC_REC, 1, f) != 1)
                break;
        fclose(f);
    }
    return n == PICS;
}

static void card(u8 *fb, int pic, int x, int y)
{
    int c, r, row;
    u8 m, v;
    for (c = 0; c < CARD_B; ++c)
        for (r = 0; r < CARD_H; ++r) {
            row = y + r;
            if (row < PLAY_TOP || row >= PLAY_BOTTOM)
                continue;
            v = pics[pic][c * CARD_H + r];
            m = 0;
            if (c == 0 && r < CORNER_ROWS) m = mtop0[r];
            if (c == 0 && r >= CARD_H - CORNER_ROWS) m = mbot0[r - (CARD_H - CORNER_ROWS)];
            if (c == 6 && r < CORNER_ROWS) m = mtop6[r];
            if (c == 6 && r >= CARD_H - CORNER_ROWS) m = mbot6[r - (CARD_H - CORNER_ROWS)];
            u8 *d = fb + row * 80 + x / 8 + c;
            *d = (u8)((*d & m) | v);
        }
    /* the colour (draw.c's pic_map) */
    for (r = 0; r < CARD_H / 8; ++r) {
        row = (y >> 3) + r;
        if (row < (PLAY_TOP >> 3) || row >= (PLAY_BOTTOM >> 3))
            continue;
        for (c = 0; c < CARD_B; ++c) {
            unsigned short *e = &cmap[row * 80 + x / 8 + c], t;
            const u8 *pm = pics[pic] + PIC_BYTES + 2 * (r * 7 + c);
            unsigned short v = (unsigned short)(pm[0] | pm[1] << 8);
            if (y & 7) {
                *e = pic == PIC_BACK ? E_BACK : pic > PIC_BACK ? E_SLOT : E_FACE;
                if ((y & 7) && r == CARD_H / 8 - 1 && row + 1 < (PLAY_BOTTOM >> 3))
                    cmap[(row + 1) * 80 + x / 8 + c] = *e;
                continue;
            }
            if ((r == 0 || r == 9) && (c == 0 || c == 6)) {
                t = *e & 0xFF;
                t |= (*e >> 8 & 3) << 8;
                if (t != T_FELT) {
                    unsigned short lo = (v & 0xFF) + (t == T_BACK ? 2 : 1);
                    v = (unsigned short)(((v & 0xFF00) + (lo & 0x300 ? 0x100 : 0)) | (lo & 0xFF));
                }
            }
            *e = v;
        }
    }
}

static int picture(u8 c)
{
    return UP(c) ? CARDID(c) : PIC_BACK;
}

/* the play area of fb (80 bytes a row, 480 rows) */
void render(u8 *fb)
{
    static const u8 pat[4] = { 0x88, 0x00, 0x22, 0x00 };
    u8 s, i, n;
    u16 x, y;
    int y0;
    (void)pat;                  /* (the Classic theme's table) */
    for (y0 = PLAY_TOP; y0 < PLAY_BOTTOM; ++y0)
        memset(fb + y0 * 80, 0, 80);
    for (y0 = 0; y0 < 60 * 80; ++y0)
        cmap[y0] = E_FELT;
    layout_init();
    for (s = 0; s < nstacks; ++s) {
        n = s_n[s];
        if (l_fan[s] == FAN_HIDDEN)
            continue;
        if (!n) {
            int p = PIC_EMPTY;
            switch (s_kind[s]) {
            case K_FOUND: p = PIC_FOUND + s_suit[s]; break;
            case K_SPIDERF: p = PIC_KING; break;
            case K_PYRROW: p = -1; break;
            case K_TALON: p = sol_can_deal() ? PIC_REDEAL : PIC_EMPTY; break;
            }
            if (p >= 0)
                card(fb, p, l_x[s], l_y[s]);
            continue;
        }
        i = 0;
        if (l_fan[s] == FAN_NONE)
            i = n - 1;
        if (l_fan[s] == FAN_RIGHT && n > l_show[s])
            i = n - l_show[s];
        for (; i < n; ++i) {
            card_xy(s, i, &x, &y);
            card(fb, picture(s_c[s][i]), x, y);
        }
    }
}

/* a state from dump.c's lines */
int read_state(const char *path)
{
    FILE *f = fopen(path, "r");
    char line[1000], name[20], *p;
    int g, round, mv, w, s;
    unsigned long d;
    if (!f || !fgets(line, sizeof line, f))
        return 0;
    if (sscanf(line, "game %d deal %lu round %d moves %d won %d", &g, &d, &round, &mv, &w) != 5)
        return 0;
    sol_new((u8)g, (u32)d);
    round_no = (u8)round;
    moves = (u16)mv;
    won = (u8)w;
    while (fgets(line, sizeof line, f)) {
        p = strchr(line, ':');
        if (!p)
            continue;
        *p++ = 0;
        if (!strcmp(line, "talon")) strcpy(name, "t");
        else if (!strcmp(line, "waste")) strcpy(name, "w");
        else if (!strncmp(line, "found", 5)) snprintf(name, sizeof name, "f%.8s", line + 5);
        else if (!strncmp(line, "row", 3)) snprintf(name, sizeof name, "r%.8s", line + 3);
        else snprintf(name, sizeof name, "v%.8s", line + 7);
        s = stack_id(name);
        if (s < 0)
            return 0;
        s_n[s] = 0;
        while (*p == ' ') {
            static const char R[] = "A23456789TJQK", S[] = "CSHD";
            char r = p[1], su = p[2];
            int up = r < 'a' && su < 'a';
            if (r >= 'a') r -= 32;
            if (su >= 'a') su -= 32;
            s_c[s][s_n[s]++] = (u8)((strchr(S, su) - S) * 13 + (strchr(R, r) - R)) | (up ? FACEUP : 0);
            p += 3;
        }
    }
    fclose(f);
    return 1;
}
