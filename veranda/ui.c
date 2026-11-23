/*
 * Veranda Solitaire: the program. Veranda's menu bar (Solitaire,
 * Game, Edit, Options: the kernel's, DECISIONS V74, V79), the felt and the
 * status bar (draw.c); the mouse: press on a card and drag its run (an
 * outline follows the pointer), release to drop it where the engine's
 * closest stack takes it; a click deals or plays (PySolFC's click
 * handlers), a double click sends a card home; the status line; the
 * moves' log in RAM banks (undo and redo as far as memory goes); the
 * statistics in /VERANDA/.SYSTEM; the win's bouncing cards.
 *
 * This file is overlay 1 (make: --code-name OVERLAY1): it runs in place
 * at $A000 of its RAM bank, which main.c maps for good as appBank, so
 * Veranda's calls to the program find it; its strings, tables and
 * variables are main RAM's (the kernel reads them there).
 *
 * Copyright (C) 2026 the Veranda Solitaire authors. GPL-3.0-or-later
 * (see COPYING); the rules are PySolFC's (see NOTICE).
 */
#include <string.h>
#include "ui.h"
#include "cards.h"
#include <vrpic.h>      /* Look 3: the cards' colour in tiles (V105) */

static u8 opt_draw3;

/*---------------------------------------------------------------
  statistics: a file in the system's data folder
 ---------------------------------------------------------------*/
struct stat { u16 played, won, streak, best, btime, bmoves; };
static struct {
    char magic[4];
    u8 version, game, autodrop, draw3;
    struct stat g[NGAMES];
} stats;
static const char stats_name[] = "Veranda Solitaire Stats";
static u8 counted;              /* this deal counted as played */

/* the system's data folder, /VERANDA/.SYSTEM (Veranda's vrpath.h:
   DECISIONS V71; the root without /VERANDA) */
static void sysdir(void)
{
    vr_cd(VR_DATA);
}

/* the desk accessories: with the applications, /VERANDA/APPS */
static void appsdir(void)
{
    vr_cd(VR_APPS);
}

static void backdir(void)
{
    ChDir(curDirName[0] ? (void *)curDirName : "/");
}

static void stats_load(void)
{
    u8 h;
    sysdir();
    h = FOpen((void *)stats_name, 'r');
    if (h >= 2) {
        FRead(h, &stats, sizeof stats);
        FClose(h);
    }
    backdir();
    if (memcmp(stats.magic, "VSOL", 4) || stats.version != 1 || stats.game >= NGAMES) {
        memset(&stats, 0, sizeof stats);
        memcpy(stats.magic, "VSOL", 4);
        stats.version = 1;
    }
}

static void stats_save(void)
{
    u8 h;
    stats.game = gid;
    stats.autodrop = opt_autodrop;
    stats.draw3 = opt_draw3;
    sysdir();
    h = FOpen((void *)stats_name, 'w');
    if (h >= 2) {
        FWrite(h, &stats, sizeof stats);
        FClose(h);
    }
    backdir();
}

/* a deal left unwon after a move: the streak ends */
static void abandon(void)
{
    if (counted && !won) {
        stats.g[gid].streak = 0;
        stats_save();
    }
    counted = 0;
}

/*---------------------------------------------------------------
  starting a deal
 ---------------------------------------------------------------*/
static void drag_cancel(void);

static void start(u8 g, u32 deal)
{
    drag_cancel();
    abandon();
    busy = 1;
    sol_new(g, deal);
    secs = 0;
    CLOCK_RESET();
    draw_all();
    CLOCK_LOG("DEAL");
    status_all();
    busy = 0;
}

static u32 new_number(void)
{
    return (u32)(random % 32000) + 1;
}

/*---------------------------------------------------------------
  the dialog boxes (own position, the UI font)
 ---------------------------------------------------------------*/
static char l1[48], l2[48], l3[48], l4[48];

#define BOX 130, 251, 140, 0, 243, 1    /* top, bottom, left 140, right 499 */
static const u8 box_info[] = {
    1, BOX,
    DBVARSTR, 16, 20, 0x0C,
    DBVARSTR, 16, 38, 0x0E,
    DBVARSTR, 16, 56, 0x10,
    DBVARSTR, 16, 74, 0x12,
    OK, 19, 92,
    0
};
static const u8 box_ask[] = {
    1, BOX,
    DBVARSTR, 16, 20, 0x0C,
    DBVARSTR, 16, 38, 0x10,
    DBGETSTRING, 16, 50, 0x0E, 10,
    OK, 10, 92,
    CANCEL, 28, 92,
    0
};

static void info(void)
{
    r5 = (u16)l1;
    r6 = (u16)l2;
    r7 = (u16)l3;
    r8 = (u16)l4;
    DoDlgBox((void *)box_info);
}

static char *dec(char *p, u32 v)
{
    char d[11];
    u8 n = 0;
    do {
        d[n++] = (char)('0' + (u8)(v % 10));
        v /= 10;
    } while (v);
    while (n)
        *p++ = d[--n];
    *p = 0;
    return p;
}

static char *mmss(char *p, u16 s)
{
    p = dec(p, s / 60);
    *p++ = ':';
    *p++ = (char)('0' + (s % 60) / 10);
    *p++ = (char)('0' + s % 10);
    *p = 0;
    return p;
}

static void stats_lines(void)
{
    struct stat *t = &stats.g[gid];
    char *p;
    strcpy(l1, G->name);
    p = dec(strcpy(l2, "Played ") + 7, t->played);
    p = dec(strcpy(p, ", won ") + 6, t->won);
    if (t->played) {
        strcpy(p, " (");
        p = dec(p + 2, (u32)t->won * 100 / t->played);
        strcpy(p, "%)");
    }
    p = dec(strcpy(l3, "Winning streak ") + 15, t->streak);
    p = dec(strcpy(p, ", best ") + 7, t->best);
    if (t->btime) {
        p = mmss(strcpy(l4, "Best time ") + 10, t->btime);
        dec(strcpy(p, ", fewest moves ") + 15, t->bmoves);
    } else
        strcpy(l4, "No win yet");
}

/*---------------------------------------------------------------
  the cards' palette rows (Look 3), light or dark as the theme
 ---------------------------------------------------------------*/
static u8 tile_bank[TILE_RECORDS];
static const u16 pal_light[2][16] = { PAL2_LIGHT, PAL3_LIGHT };
static const u16 pal_dark[2][16] = { PAL2_DARK, PAL3_DARK };

static void colours(void)
{
    const u16 *p = theme.dark ? pal_dark[0] : pal_light[0];
    u16 a = (u16)(VR_PIC_PALETTE + ROW_RED * 32);
    u8 i;
    if (!look)
        return;
    *(volatile u8 *)0x9F25 = 0;
    *(volatile u8 *)0x9F20 = (u8)a;
    *(volatile u8 *)0x9F21 = (u8)(a >> 8);
    *(volatile u8 *)0x9F22 = 0x11;
    for (i = 0; i < 32; ++i, ++p) {
        *(volatile u8 *)0x9F23 = (u8)*p;
        *(volatile u8 *)0x9F23 = (u8)(*p >> 8);
    }
}

/*---------------------------------------------------------------
  the menus
 ---------------------------------------------------------------*/
struct item { const char *text; u8 type; void *p; };
struct menu2 { u8 top, bottom; u16 left, right; u8 flags; struct item i[2]; };
struct menu3 { u8 top, bottom; u16 left, right; u8 flags; struct item i[3]; };
struct menu6 { u8 top, bottom; u16 left, right; u8 flags; struct item i[6]; };
struct menu7 { u8 top, bottom; u16 left, right; u8 flags; struct item i[7]; };
struct menu9 { u8 top, bottom; u16 left, right; u8 flags; struct item i[9]; };
struct menu4 { u8 top, bottom; u16 left, right; u8 flags; struct item i[4]; };

/* Veranda Look 2's bar (the kernel's geometry, V74): 16 lines; its lists
   open under their item, 16 lines an item (only a table's width counts) */
#define BAR_H 16
#define IH 16
#define DA_MAX 6

static char da_names[DA_MAX][17];
static u8 n_da;

static void m_about(void), m_da0(void), m_da1(void), m_da2(void), m_da3(void), m_da4(void), m_da5(void);
static void m_new(void), m_restart(void), m_number(void), m_stats(void), m_quit(void);
static void m_undo(void), m_redo(void), m_auto(void), m_draw1(void), m_draw3(void);
static void m_g0(void), m_g1(void), m_g2(void), m_g3(void), m_g4(void), m_g5(void), m_g6(void),
    m_g7(void), m_g8(void);

static const char t_auto0[] = "  Auto-drop", t_auto1[] = "* Auto-drop";
static const char t_d10[] = "\x1f  Draw 1", t_d11[] = "\x1f* Draw 1";
static const char t_d30[] = "  Draw 3", t_d31[] = "* Draw 3";
/* (Look 3's menus: "\x1f" starts a group, a dotted line above the item;
   after a TAB the item's key, at the list's right) */
static char da_first[18];               /* the first accessory: a group */

/* the games of the list: Klondike's draw comes from the options */
static const u8 list_game[9] = { 0, 2, 3, 4, 5, 6, 7, 8, 9 };

static struct menu7 mgeos = { BAR_H, BAR_H + IH, 0, 0, VERTICAL | 1, {
    { "About Veranda Solitaire", MENU_ACTION, m_about },
    { da_first, MENU_ACTION, m_da0 }, { da_names[1], MENU_ACTION, m_da1 },
    { da_names[2], MENU_ACTION, m_da2 }, { da_names[3], MENU_ACTION, m_da3 },
    { da_names[4], MENU_ACTION, m_da4 }, { da_names[5], MENU_ACTION, m_da5 } } };
static struct menu9 mchoose = { BAR_H + 3 * IH, BAR_H + 12 * IH, 0, 0, VERTICAL | 9, {
    { "Klondike", MENU_ACTION, m_g0 }, { "FreeCell", MENU_ACTION, m_g1 },
    { "Spider (1 suit)", MENU_ACTION, m_g2 }, { "Spider (2 suits)", MENU_ACTION, m_g3 },
    { "Spider (4 suits)", MENU_ACTION, m_g4 }, { "Golf", MENU_ACTION, m_g5 },
    { "Pyramid", MENU_ACTION, m_g6 }, { "Yukon", MENU_ACTION, m_g7 },
    { "Russian Solitaire", MENU_ACTION, m_g8 } } };
static struct menu6 mgame = { BAR_H, BAR_H + 6 * IH, 0, 0, VERTICAL | 6, {
    { "New deal\tAlt N", MENU_ACTION, m_new }, { "Restart deal\tAlt R", MENU_ACTION, m_restart },
    { "Deal number...\tAlt D", MENU_ACTION, m_number }, { "\x1f" "Choose game", SUB_MENU, &mchoose },
    { "Statistics...", MENU_ACTION, m_stats }, { "\x1fQuit", MENU_ACTION, m_quit } } };
static struct menu2 medit = { BAR_H, BAR_H + 2 * IH, 0, 0, VERTICAL | 2, {
    { "Undo\tAlt Z", MENU_ACTION, m_undo }, { "Redo\tAlt Y", MENU_ACTION, m_redo } } };
static struct menu3 mopts = { BAR_H, BAR_H + 3 * IH, 0, 0, VERTICAL | 3, {
    { t_auto0, MENU_ACTION, m_auto }, { t_d11, MENU_ACTION, m_draw1 },
    { t_d30, MENU_ACTION, m_draw3 } } };
static const char *const bar_names[4] = { "Solitaire", "Game", "Edit", "Options" };
static struct menu4 mbar = { 0, BAR_H, 0, 0, HORIZONTAL | UN_CONSTRAINED | 4, {
    { "Solitaire", SUB_MENU, &mgeos }, { "Game", SUB_MENU, &mgame },
    { "Edit", SUB_MENU, &medit }, { "Options", SUB_MENU, &mopts } } };

/* an item's width: a group's mark none, a TAB 24 (the key's gap) */
static u16 strw(const char *s)
{
    u16 w = 0;
    for (; *s; ++s)
        w += *s == '\t' ? 24 : *s == 0x1F ? 0 : GetCharWidth(*s);
    return w;
}

static u16 widest(const struct item *it, u8 n)
{
    u16 w = 0, k;
    while (n--) {
        k = strw((it++)->text);
        if (k > w)
            w = k;
    }
    return w + 12;
}

static void menu_marks(void)
{
    mopts.i[0].text = opt_autodrop ? t_auto1 : t_auto0;
    mopts.i[1].text = opt_draw3 ? t_d10 : t_d11;
    mopts.i[2].text = opt_draw3 ? t_d31 : t_d30;
}

/* the lists' widths from the UI font's (the kernel places them: under
   their bar item, Choose game beside its item); the bar's table ends
   where the kernel ends its items (vr_bar_end: it takes clicks up to it) */
static void menu_layout(void)
{
    static struct menu2 *const subs[4] = {
        (struct menu2 *)&mgeos, (struct menu2 *)&mgame, (struct menu2 *)&medit, (struct menu2 *)&mopts };
    static const u8 counts[4] = { 7, 6, 2, 3 };
    u8 i;
    da_first[0] = 0x1F;
    strcpy(da_first + 1, da_names[0]);
    mgeos.flags = (u8)(VERTICAL | (1 + n_da));
    mgeos.bottom = (u8)(BAR_H + (1 + n_da) * IH);
    for (i = 0; i < 4; ++i) {
        subs[i]->left = 0;
        subs[i]->right = widest(subs[i]->i, i ? counts[i] : 1 + n_da);
    }
    mchoose.left = 0;
    mchoose.right = widest(mchoose.i, 9);
    mbar.right = vr_bar_end(bar_names, 4);
    UseFont(FONT_UI);                   /* (vr_bar_end ends in the system font) */
}

/* the kernel's menu bar (Veranda Look 2: the mark, Solitaire in bold, the
   clock and the speaker at the right; open menus in the theme's
   highlight colour) */
static void menus(void)
{
    menu_marks();
    theme_menus();
    DoMenu(&mbar, 0);
}

static void close_menu(void)
{
    GotoFirstMenu();
    drag_cancel();      /* (after the menu gave back what it covered) */
}

/*---------------------------------------------------------------
  after the engine moved: the screen, the status, the counts
 ---------------------------------------------------------------*/
static void win_start(void);

static void after(u8 done)
{
    if (!done)
        return;
    drag_cancel();      /* (the table changes under it: no outline left) */
    busy = 1;
    CLOCK_RESET();
    draw_dirty();
    CLOCK_LOG("MOVE");
    if (moves && !counted) {
        counted = 1;
        ++stats.g[gid].played;
        stats_save();
    }
    CLOCK_RESET();
    draw_status();
    CLOCK_LOG("STATUS");
    busy = 0;
#ifdef SOL_REALMOUSE
    sol_test_key();     /* the state after each move */
#endif
    if (won)
        win_start();
}

/*---------------------------------------------------------------
  the menus' routines
 ---------------------------------------------------------------*/
static void m_about(void)
{
    close_menu();
    strcpy(l1, "Veranda Solitaire 1.0");
    strcpy(l2, "Ten games; the rules are PySolFC's");
    strcpy(l3, "(GPL-3.0; see NOTICE). The cards");
    strcpy(l4, "are drawn for Veranda.");
    info();
}

static void da(u8 i)
{
#ifdef SOL_TEST
    /* the session's script is mouseSource, which an accessory's start and
       end clear: kept; the accessory gets a 'q' (the calculator closes) */
    u16 src = mouseSource;
    asm("lda #$71");
    asm("jsr $FEC3");           /* the ROM's kbdbuf_put */
#endif
    /* the script stands still until the table is back: the kernel keeps
       the zero page when the accessory starts and puts it back when it
       ends, and since Veranda's V83 the program is packed away first (half
       a second): script entries played meanwhile (the player's place is
       in the zero page) would be played again */
    busy = 1;
    close_menu();
    appsdir();
    GetFile(da_names[i], 0, 0, 0, 0);
    backdir();
#ifdef SOL_TEST
    mouseSource = src;
#endif
    UseFont(FONT_UI);
    colours();
    draw_all();
    status_all();
    busy = 0;
}
static void m_da0(void) { da(0); }
static void m_da1(void) { da(1); }
static void m_da2(void) { da(2); }
static void m_da3(void) { da(3); }
static void m_da4(void) { da(4); }
static void m_da5(void) { da(5); }

static void m_new(void)
{
    close_menu();
    start(gid, new_number());
}

static void m_restart(void)
{
    close_menu();
    start(gid, deal_no);
}

static void m_number(void)
{
    static char num[12];
    u32 v = 0;
    u8 i, r;
    close_menu();
    strcpy(l1, "Deal number (1 to 2147483647):");
    strcpy(l3, "");
    num[0] = 0;
    r5 = (u16)l1;
    r6 = (u16)num;
    r7 = (u16)l3;
    r = DoDlgBox((void *)box_ask);
    if (r != OK && r != DBGETSTRING)
        return;
    for (i = 0; num[i]; ++i) {
        if (num[i] < '0' || num[i] > '9' || v > 214748364UL)
            return;
        v = v * 10 + (u8)(num[i] - '0');
    }
    if (!v || v > 2147483647UL)
        return;
    start(gid, v);
}

static void choose(u8 k)
{
    u8 g = list_game[k];
    close_menu();
    if (g == 0 && opt_draw3)
        g = 1;
    start(g, new_number());
    stats_save();
}
static void m_g0(void) { choose(0); }
static void m_g1(void) { choose(1); }
static void m_g2(void) { choose(2); }
static void m_g3(void) { choose(3); }
static void m_g4(void) { choose(4); }
static void m_g5(void) { choose(5); }
static void m_g6(void) { choose(6); }
static void m_g7(void) { choose(7); }
static void m_g8(void) { choose(8); }

static void m_stats(void)
{
    close_menu();
    stats_lines();
    info();
}

static void m_quit(void)
{
    close_menu();
    abandon();
    stats_save();
    leave();
}

static void undo(void)
{
    busy = 1;
    after(act_undo());
    busy = 0;
}

static void redo(void)
{
    busy = 1;
    after(act_redo());
    busy = 0;
}

static void m_undo(void) { close_menu(); undo(); }
static void m_redo(void) { close_menu(); redo(); }

static void m_auto(void)
{
    close_menu();
    opt_autodrop ^= 1;
    menu_marks();
    stats_save();
}

static void draw_mode(u8 three)
{
    close_menu();
    opt_draw3 = three;
    menu_marks();
    stats_save();
    if (gid <= 1 && gid != three)
        start(three, deal_no);      /* the same deal, drawn the other way */
}
static void m_draw1(void) { draw_mode(0); }
static void m_draw3(void) { draw_mode(1); }

/*---------------------------------------------------------------
  the win: cards bounce off the foundations, leaving trails
 ---------------------------------------------------------------*/
static u8 anim, last_frame, a_k, a_pic;
static int a_x, a_y16, a_vx, a_vy16;

/* the next card to fly: the foundations' top cards in turn */
static u8 anim_next(void)
{
    u8 f, i;
    while (a_k < ncards) {
        f = st_f0 + a_k % n_found;
        i = (u8)(a_k / n_found);
        ++a_k;
        if (i < s_n[f]) {
            a_pic = mono_pic(CARDID(s_c[f][s_n[f] - 1 - i]));
            a_x = l_x[f] >> 3;
            a_y16 = l_y[f] << 4;
            a_vx = (a_k & 1) ? -1 : 1;
            if (a_x < 8)
                a_vx = 1;
            if (a_x > 64)
                a_vx = -1;
            a_vy16 = -(int)(16 + (a_k * 7) % 40);
            return 1;
        }
    }
    return 0;
}

static void win_end(void);

#ifdef SOL_TEST
static u8 logged;               /* the first frames timed */
#endif

static void anim_step(void)
{
    if (a_x < 0 || a_x > 80 - CARD_B) {
        if (!anim_next()) {
            win_end();
            return;
        }
    }
    a_vy16 += 6;
    a_y16 += a_vy16;
    if (a_y16 > (PLAY_BOTTOM - CARD_H) << 4) {
        a_y16 = (PLAY_BOTTOM - CARD_H) << 4;
        a_vy16 = -(a_vy16 * 3) / 4;
    }
#ifdef SOL_TEST
    if (logged < 3)
        CLOCK_RESET();
#endif
    draw_pic(a_pic, (u16)a_x << 3, (u16)(a_y16 >> 4), CARD_H);
#ifdef SOL_TEST
    if (logged < 3) {
        ++logged;
        CLOCK_LOG("WINFRAME");
    }
#endif
    a_x += a_vx;
}

static void win_start(void)
{
    struct stat *t = &stats.g[gid];
    ++t->won;
    if (++t->streak > t->best)
        t->best = t->streak;
    if (!t->btime || secs < t->btime)
        t->btime = secs ? secs : 1;
    if (!t->bmoves || moves < t->bmoves)
        t->bmoves = moves;
    stats_save();
    counted = 0;
    felt_off();                 /* (the cards fly over normal paper) */
    a_k = 0;
    a_x = -1;
    anim = 1;
    last_frame = frames;
    LogText("SOL WON");
}

static void win_end(void)
{
    char *p;
    anim = 0;
    draw_all();
    status_all();
    strcpy(l1, "You won!");
    strcpy(l2, G->name);
    p = l2 + strlen(l2);
    p = dec(strcpy(p, " #") + 2, deal_no);
    p = mmss(strcpy(l3, "in ") + 3, secs);
    dec(strcpy(p, " and ") + 5, moves);
    strcat(l3, " moves");
    p = dec(strcpy(l4, "Won ") + 4, stats.g[gid].won);
    p = dec(strcpy(p, " of ") + 4, stats.g[gid].played);
    strcpy(p, " games");
    LogText("SOL WIN BOX");
    info();
}

/*---------------------------------------------------------------
  the mouse: press, drag, release; double clicks
 ---------------------------------------------------------------*/
static u8 drag, d_s, d_n, last_s = ANY, last_i = ANY;
static u16 px, py, ox, oy, oh;
static int fx, fy;              /* the outline's corner shown */

static void pointer(u16 *x, u16 *y)
{
    do {
        *x = mouseXPos;
        *y = mouseYPosW;
    } while (*x != mouseXPos || *y != mouseYPosW);
}

static void outline(int x, int y)
{
    xor_frame(x, y, x + CARD_W - 1, y + oh - 1);
    xor_frame(x + 1, y + 1, x + CARD_W - 2, y + oh - 2);
}

/* the drag given up: its outline off the screen, the cards where they
   are. The outline is inverted pixels: drawn over by a redraw and then
   inverted again it would stay on the table (the hardware report's line
   through a foundation), so every redraw ends a drag first */
static void drag_cancel(void)
{
    if (!drag)
        return;
    drag = 0;
    outline(fx, fy);
}

static void press(void)
{
    u16 x, y, x1, y1;
    u8 s, i, dbl;
    if (anim) {
        if (!(mouseData & 0x80))
            win_end();
        return;
    }
    pointer(&x, &y);
    if (drag && !(mouseData & 0x80)) {
        /* a press while a drag is on: its release was never seen (the
           release and this press came while the program was busy, and
           the kernel reports the button as it is now; or the mouse lost
           the release). The drag is given up, then this press counts */
        LogText("SOL DRAG MISSED RELEASE");
        drag_cancel();
    }
    if (mouseData & 0x80) {         /* the release */
        int dx, dy;
        if (!drag || menuNumber)    /* (a menu over the outline: it stays) */
            return;
        drag = 0;
        outline(fx, fy);
        dx = (int)x - (int)px;
        dy = (int)y - (int)py;
        busy = 1;
        if (dx > -10 && dx < 10 && dy > -10 && dy < 10)
            after(act_click(d_s));          /* not moved: a click */
        else {
            s = closest_stack((u16)((int)ox + dx), (u16)((int)oy + dy));
            if (s != ANY)
                after(act_move(d_s, s, d_n));
        }
        busy = 0;
        return;
    }
    if (y < PLAY_TOP || y >= PLAY_BOTTOM)
        return;
    s = hit_test(x, y, &i);
    dbl = (u8)(dblClickCount && s == last_s && i == last_i && s != ANY);
    last_s = s;
    last_i = i;
    dblClickCount = 30;
    if (s == ANY)
        return;
    busy = 1;
    if (dbl) {
        last_s = ANY;
        after(act_double(s));
    } else if (s == st_talon || i == ANY || !UP(s_c[s][i])) {
        after(act_click(s));
    } else if (sol_can_move(s, s_n[s] - i)) {
        drag = 1;
        d_s = s;
        d_n = s_n[s] - i;
        px = x;
        py = y;
        card_xy(s, i, &ox, &oy);
        card_xy(s, s_n[s] - 1, &x1, &y1);
        oh = y1 - oy + CARD_H;
        fx = ox;
        fy = oy;
        outline(fx, fy);
    } else {
        after(act_click(s));
    }
    busy = 0;
}

static void poll(void)
{
    u16 x, y;
    int nx, ny;
    if (look && vr_pic_back()) {
        /* back from aside (Alt + Tab): the theme may be dark now */
        theme_load();
        theme_palette();
        colours();
    }
    if (anim) {
        if (frames != last_frame) {
            last_frame = frames;
            anim_step();
        }
        return;
    }
    if (!drag || menuNumber)    /* (a menu open: the outline is under it) */
        return;
    pointer(&x, &y);
    nx = (int)ox + ((int)x - (int)px);
    ny = (int)oy + ((int)y - (int)py);
    if (nx == fx && ny == fy)
        return;
    CLOCK_RESET();
    outline(fx, fy);
    fx = nx;
    fy = ny;
    outline(fx, fy);
    CLOCK_LOG("DRAG");
}

/*---------------------------------------------------------------
  keys: Alt + N new deal, R restart, Z undo, Y redo, D deal number
 ---------------------------------------------------------------*/
static void key(void)
{
    u8 k = keyData;
#ifdef SOL_TEST
    if ((k | 0x20) == (SHORTCUT | 'w')) {
        sol_test_key();
        return;
    }
#endif
    if (anim) {
        win_end();
        return;
    }
    if (drag)
        return;
    if (!(k & SHORTCUT))
        return;
    switch ((u8)(k & 0x7F) | 0x20) {
    case 'z': undo(); break;
    case 'y': redo(); break;
    case 'n': start(gid, new_number()); break;
    case 'r': start(gid, deal_no); break;
    case 'd': m_number(); break;
    }
}

/* the second ticker (a process, every 60 frames) */
static void tick(void)
{
    if (moves && !won && !anim && !menuNumber) {
        ++secs;
        draw_time();
    }
#ifdef SOL_SLOWTICK
    if (moves) {
        sol_slow = 0;           /* (the gate's mouse script waits on it) */
        sol_test_slow();
    }
#endif
}

struct proc { void *f; u16 frames; };
static struct proc procs[1] = { { tick, 60 } };

void ui_main(void)
{
    u8 i, g;
    theme_load();
    theme_palette();            /* Veranda's palette (V74) */
    look = !theme_none();       /* (Classic: no colours) */
    uiFont = FONT_UI;
    UseFont(FONT_UI);
    dispBufferOn = ST_WR_FORE;
    for (i = 0; i < PIC_RECORDS + TILE_RECORDS; ++i) {
        g = ovl_bank(i + 2);
        if (!g) {
            LogText("SOL NO PICTURES");
            leave();
        }
        if (i < PIC_RECORDS)
            card_bank[i] = g;
        else
            tile_bank[i - PIC_RECORDS] = g;
    }
    if (look) {
        /* Look 3: the cards' colour in VERA's tiles under the bitmap; the
           ink on both screens (menus come back from the back one) */
        vr_pic_on();
        for (i = 0; i < TILE_RECORDS; ++i)
            vr_vram_load(tile_bank[i]);
        colours();
        dispBufferOn = ST_WR_FORE | ST_WR_BACK;
        bl_both = 1;
        status_font = LoadFont("Gate Sans", 9);
        if (status_font == FONT_NONE)
            status_font = FONT_UI;
    }
    stats_load();
    opt_autodrop = stats.autodrop;
    opt_draw3 = stats.draw3;
    appsdir();
    n_da = (u8)(DA_MAX - FindFTypes(da_names, DESK_ACC, DA_MAX, 0));
    if (n_da > DA_MAX)
        n_da = 0;
    backdir();
    SetPattern(0);
    WRectangle(0, 0, 639, 479);
    menu_layout();
    menus();
    g = stats.game;
    if (g <= 1)
        g = opt_draw3;
#ifdef SOL_TEST
    start(SOL_GAME, SOL_DEAL);
#else
    start(g, new_number());
#endif
    otherPressVec = VR_FN(press);
    keyVector = VR_FN(key);
    appMain = VR_FN(poll);
    InitProcesses(procs, 1);
    RestartProcess(0);
    LogText("SOLITAIRE RUNNING");
#ifdef SOL_TEST
    sol_test_start();
#endif
}
