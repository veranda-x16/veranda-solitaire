/*
 * Veranda Solitaire: scripted sessions for the emulator, made with the
 * engine and the layout the X16 runs (so every press lands on the card
 * meant, and every drop on the stack meant), and the states the host
 * engine reaches at each snapshot.
 *
 *   mkscript PLAN OUTDIR
 *
 * OUTDIR/script.bin: the test build's script (veranda/test/play.s's
 * entries; STOP: none after that frame, the end there), OUTDIR/script.h
 * the same as text, with "/ * shot NAME FRAME * /" lines; OUTDIR/expectN: the
 * state at the program's Nth snapshot (Alt + W); OUTDIR/mouse.txt: the
 * real-mouse script (x16emu -mousescript) for the plan's "mouse" drag, or
 * for everything after "realmouse".
 *
 * The plan, a line each:
 *   game G DEAL        the session's game and deal (the test build's)
 *   auto N             N actions a simple player chooses: home a card,
 *                      uncover one, play the waste, else deal
 *   moves FILE [drag]  a move list (m FROM TO N, c S): drags, moves home
 *                      as double clicks (with "drag": as drags too), clicks
 *   undo | redo        Alt + Z | Alt + Y
 *   state              Alt + W: a snapshot (expectN)
 *   menu T I           menu T (0 Solitaire, 1 Game, 2 Edit, 3 Options), item I,
 *                      by the mouse (the host does restart, undo, redo,
 *                      auto-drop, draw 1 and 3 too)
 *   choose K           game, choose game, the K-th game (a dealno must
 *                      follow: the deal is random)
 *   ok                 a click on a dialog box's OK
 *   dealno N           Alt + D, N typed, Return
 *   drag FROM TO N     a drag (stack names: t w f0 r0 v0 ...)
 *   shot NAME [K]      a screenshot in the middle of the next drag, at its
 *                      step K of 6 (default 3; the state then:
 *                      OUTDIR/shot-NAME.state, the outline's corner
 *                      OUTDIR/shot-NAME.pos)
 *   mouse              the next drag also as the real-mouse script
 *   realmouse          from here on every press, drag and click also as the
 *                      real-mouse script (PS/2 movements from 0, 0; a pause
 *                      after each action); "end" ends it
 *   dbl S GAP          a double click on stack S's top card, GAP frames
 *                      between the first release and the second press
 *   shotnow NAME       a screenshot at this frame
 *   click X Y          a click at X, Y
 *   clickstack S       a click on stack S (its top card, or its place)
 *   wait F             F frames
 *   end F              the end, F frames on
 *
 * GPL-3.0-or-later (see COPYING).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "host.h"

static FILE *out;
static const char *dir;
static int f = 10;              /* the frame */
static int nstate;
static int shot_next, shot_step = 3;
static char shot_name[32];
static int mouse_next;
static FILE *mouse;
static int cur_x = 320, cur_y = 240;
static int realmouse, rm_x, rm_y, rm_b;   /* "realmouse": every press too */

/* realmouse: the frames after each action (the test build saves the
   state and the screen after each move: no press is lost meanwhile) */
#define RM_PAUSE 90
#define GRAB_X 28
#define GRAB_Y 5

static void die(const char *what);

/* the menus' geometry: Veranda Look 2's menu bar (the kernel's, its
   DECISIONS V74, V79; Veranda's tests/look_menu.py): item 0 (bold, a
   pixel more a character) after the mark's 24 pixels, the others 6 pixels
   from their text each side, each item's end rounded up to a card; a list
   under its bar item (from its left), on cards, 16 lines an item from line
   16, as wide as its table (veranda/ui.c's menu_layout) + 8 rounded up; a
   list's sub list beside its item (from the list's right end, on a card;
   its top the item's). The widths are the Veranda UI font's: SOL_UIFONT
   names the font (build/vsrc's mkfont.py --ui) */
static int uiw[96];
static int m_left[4], m_right[4], t_mid[4], c_left;
#define BAR_H 16
#define IH 16
static const char *const titles[4] = { "Solitaire", "Game", "Edit", "Options" };

static int strw(const char *t)
{
    int w = 0;
    for (; *t; ++t)
        w += *t == '\t' ? 24 : (*t >= 32 && *t < 128) ? uiw[*t - 32] : 0;   /* (ui.c's strw) */
    return w;
}

static int widest(const char *const *t, int n)
{
    int w = 0, k;
    while (n--)
        if ((k = strw(*t++)) > w)
            w = k;
    return w + 12;
}

static void menu_geometry(void)
{
    static const char *const geos[] = { "About Veranda Solitaire" };
    static const char *const game[] = { "New deal\tAlt N", "Restart deal\tAlt R", "Deal number...\tAlt D",
                                        "\x1f" "Choose game", "Statistics...", "\x1fQuit" };
    static const char *const edit[] = { "Undo\tAlt Z", "Redo\tAlt Y" };
    static const char *const opts[] = { "  Auto-drop", "\x1f* Draw 1", "  Draw 3" };
    static const char *const choose[] = { "Klondike", "FreeCell", "Spider (1 suit)", "Spider (2 suits)",
        "Spider (4 suits)", "Golf", "Pyramid", "Yukon", "Russian Solitaire" };
    static const char *const *const subs[4] = { geos, game, edit, opts };
    static const int counts[4] = { 1, 6, 2, 3 };
    const char *font = getenv("SOL_UIFONT");
    unsigned char d[2000];
    FILE *ff;
    int b = 0, i, n;
    if (!font || !(ff = fopen(font, "rb")))
        die("SOL_UIFONT: the Veranda UI font (mkfont.py --ui)");
    n = (int)fread(d, 1, sizeof d, ff);
    fclose(ff);
    if (n < 8 + 2 * 97)
        die("SOL_UIFONT: too short");
    for (i = 0; i < 96; ++i)
        uiw[i] = (d[8 + 2 * (i + 1)] | d[9 + 2 * (i + 1)] << 8) - (d[8 + 2 * i] | d[9 + 2 * i] << 8);
    for (i = 0; i < 4; ++i) {
        int e = b + (i ? 6 + strw(titles[i]) : 24 + strw(titles[i]) + (int)strlen(titles[i]));
        e = (e + 6 + 7) & ~7;
        t_mid[i] = (b + e) / 2;
        m_left[i] = b & ~7;
        /* the list's right end (its last column + 1) */
        m_right[i] = (m_left[i] + widest(subs[i], counts[i]) + 15) & ~7;
        b = e;
    }
    c_left = m_right[1];        /* Choose game's list: beside the Game list */
    (void)choose;
}

static FILE *bin;
static int stop = 0xFFFE, entries;

/* an entry of veranda/test/play.s's script: frame, x, y | flags << 8
   (bit 0 the button, bit 6 y + 256, bit 7 a key: x is its code) */
static void entry(int fr, int x, int yf)
{
    unsigned char e[6];
    if (fr > stop && fr != 0xFFFF)
        return;
    e[0] = (unsigned char)fr;
    e[1] = (unsigned char)(fr >> 8);
    e[2] = (unsigned char)x;
    e[3] = (unsigned char)(x >> 8);
    e[4] = (unsigned char)yf;
    e[5] = (unsigned char)(yf >> 8);
    fwrite(e, 6, 1, bin);
    ++entries;
}

static void P(int x, int y, int b)
{
    fprintf(out, "P(%d, %d, %d, %d),\n", f, x, y, b);
    entry(f, x, (y & 255) | ((y >> 8) << 14) | (b << 8));
    if (realmouse) {
        if (x != rm_x || y != rm_y)
            fprintf(mouse, "%d move %d %d\n", 400 + f, x - rm_x, y - rm_y);
        if (b != rm_b)
            fprintf(mouse, "%d %s\n", 400 + f, b ? "down" : "up");
        rm_x = x;
        rm_y = y;
        rm_b = b;
    }
    cur_x = x;
    cur_y = y;
}

static void K(int code, const char *what)
{
    fprintf(out, "K(%d, %d),  /* %s */\n", f, code, what);
    entry(f, code, 0x8000);
}

static void die(const char *what)
{
    fprintf(stderr, "mkscript: %s\n", what);
    exit(1);
}

static void snapshot(void)
{
    char name[300];
    FILE *e;
    K(0x80 | 'w', "Alt + W: the state");
    f += 20;
    sprintf(name, "%s/expect%d", dir, ++nstate);
    e = fopen(name, "w");
    if (!e)
        die("cannot write an expectation");
    print_state(e, 1);
    fclose(e);
}

/* a point on card i of stack s that hits it */
static void press_point(u8 s, u8 i, int *x, int *y)
{
    u16 cx, cy;
    u8 hi, hs;
    card_xy(s, i, &cx, &cy);
    *x = cx + GRAB_X;
    *y = cy + GRAB_Y;
    hs = hit_test((u16)*x, (u16)*y, &hi);
    if (hs != s || hi != i) {
        fprintf(stderr, "stack %s card %d at %d, %d hits %d/%d\n", stack_name(s), i, *x, *y, hs, hi);
        die("a press would miss its card");
    }
}

static void click_at(int x, int y)
{
    P(x, y, 0);
    f += 2;
    P(x, y, 1);
    f += 3;
    P(x, y, 0);
    f += 12;
}

/* the top-left corner where a card dropped on t lands */
static int target(u8 t, u8 from, int *tx, int *ty)
{
    u16 x, y;
    if (s_n[t] && l_fan[t] == FAN_DOWN) {
        card_xy(t, s_n[t] - 1, &x, &y);
        y += 14;
    } else {
        x = l_x[t];
        y = l_y[t];
    }
    *tx = x;
    *ty = y;
    if (closest_stack(x, y) == t)
        return 1;
    *tx = l_x[t];
    *ty = l_y[t];
    (void)from;
    return closest_stack(l_x[t], l_y[t]) == t;
}

static void drag(u8 from, u8 n, u8 to)
{
    int x0, y0, tx, ty, x1, y1, k;
    u8 i = s_n[from] - n;
    press_point(from, i, &x0, &y0);
    if (!target(to, from, &tx, &ty))
        die("no place drops on the stack meant");
    {
        u16 cx, cy;
        card_xy(from, i, &cx, &cy);
        x1 = tx + (x0 - cx);
        y1 = ty + (y0 - cy);
    }
    P(x0, y0, 0);
    f += 2;
    P(x0, y0, 1);
    f += 3;
    for (k = 1; k <= 6; ++k) {
        P(x0 + (x1 - x0) * k / 6, y0 + (y1 - y0) * k / 6, 1);
        if (k == shot_step && shot_next) {
            char name[300];
            FILE *e;
            u16 cx, cy;
            fprintf(out, "/* shot %s %d */\n", shot_name, f + 1);
            sprintf(name, "%s/shot-%s.state", dir, shot_name);
            e = fopen(name, "w");
            if (e) {
                print_state(e, 1);
                fclose(e);
            }
            /* where the outline should be: the card's corner moved as
               the pointer was (check_screen.py outline DUMP RENDER X Y) */
            card_xy(from, i, &cx, &cy);
            sprintf(name, "%s/shot-%s.pos", dir, shot_name);
            e = fopen(name, "w");
            if (e) {
                fprintf(e, "%d %d\n", cx + (x1 - x0) * k / 6, cy + (y1 - y0) * k / 6);
                fclose(e);
            }
            shot_next = 0;
            shot_step = 3;
        }
        f += 2;
    }
    f += 2;
    P(x1, y1, 0);
    f += 14;
    if (mouse_next) {
        /* the real mouse: pushed to 0, 0, then moved by the same amounts */
        int t = 400;
        fprintf(mouse, "0 id 3\n%d move -2000 -2000\n", t);
        t += 40;
        fprintf(mouse, "%d move %d %d\n", t, x0, y0);
        t += 20;
        fprintf(mouse, "%d down\n", t);
        t += 10;
        for (k = 1; k <= 6; ++k) {
            fprintf(mouse, "%d move %d %d\n", t, (x1 - x0) / 6 + (k == 6 ? (x1 - x0) % 6 : 0),
                    (y1 - y0) / 6 + (k == 6 ? (y1 - y0) % 6 : 0));
            t += 4;
        }
        t += 6;
        fprintf(mouse, "%d up\n", t);
        t += 120;
        fprintf(mouse, "%d exit\n", t);
        mouse_next = 0;
    }
    if (!act_move(from, to, n))
        die("the engine refuses a drag");
    if (realmouse)
        f += RM_PAUSE;
}

static int dbl_gap = 4;         /* frames between the clicks */

static void dbl(u8 s)
{
    int x, y;
    press_point(s, s_n[s] - 1, &x, &y);
    P(x, y, 0);
    f += 2;
    P(x, y, 1);
    f += 3;
    P(x, y, 0);
    f += dbl_gap;
    P(x, y, 1);
    f += 3;
    P(x, y, 0);
    f += 14;
    if (!act_double(s))
        die("the engine refuses a double click");
    if (realmouse)
        f += RM_PAUSE;
}

static void click_stack(u8 s)
{
    int x, y;
    if (s_n[s])
        press_point(s, s_n[s] - 1, &x, &y);
    else {
        x = l_x[s] + GRAB_X;
        y = l_y[s] + GRAB_Y;
    }
    click_at(x, y);
    if (!act_click(s))
        die("the engine refuses a click");
    if (realmouse)
        f += RM_PAUSE;
}

/* a simple player: home, uncover, waste, deal */
static int auto_one(void)
{
    u8 s, t, to, n, i, k;
    for (s = 0; s < nstacks; ++s)
        if (sol_stack_is_drop(s) && s_n[s] && sol_can_drop(s, &to) == 1) {
            fprintf(out, "/* %s home */\n", stack_name(s));
            /* where a click plays (Pyramid's kings, Golf), a click */
            if ((G->flags & GF_PAIRS) || s_kind[s] == K_GOLFROW)
                click_stack(s);
            else
                dbl(s);
            return 1;
        }
    for (k = 0; k < n_rows; ++k) {
        s = st_r0 + k;
        for (i = 0; i < s_n[s] && !UP(s_c[s][i]); ++i)
            ;
        if (i == 0 || i >= s_n[s])
            continue;       /* nothing to uncover */
        n = s_n[s] - i;
        if (!sol_can_move(s, n))
            continue;
        for (t = st_r0; t < st_r0 + n_rows; ++t)
            if (t != s && sol_accepts(t, s, n)) {
                fprintf(out, "/* %s -> %s (%d) */\n", stack_name(s), stack_name(t), n);
                drag(s, n, t);
                return 1;
            }
    }
    /* Pyramid: a pair, the top card of one stack dropped on another */
    if (G->flags & GF_PAIRS)
        for (s = 0; s < nstacks; ++s)
            if (s_n[s] && sol_can_move(s, 1))
                for (t = 0; t < nstacks; ++t)
                    if (t != s && sol_stack_is_open(t) && sol_accepts(t, s, 1)) {
                        fprintf(out, "/* pair %s %s */\n", stack_name(s), stack_name(t));
                        drag(s, 1, t);
                        return 1;
                    }
    if (st_waste != ANY && s_n[st_waste] && sol_can_move(st_waste, 1) && !(G->flags & GF_PAIRS))
        for (t = st_r0; t < st_r0 + n_rows; ++t)
            if (sol_accepts(t, st_waste, 1)) {
                fprintf(out, "/* waste -> %s */\n", stack_name(t));
                drag(st_waste, 1, t);
                return 1;
            }
    if (sol_can_deal()) {
        fprintf(out, "/* deal */\n");
        click_stack(st_talon);
        return 1;
    }
    return 0;
}

/* menu t's item i through the mouse: the title pressed, the pointer then
   inside the menu (where Veranda puts it; outside, the menu would close),
   the item pressed */
static void menu_at(int t, int i, int x, int sub);

static void menu(int t, int i)
{
    menu_at(t, i, m_left[t] + 10, 0);
}

/* (x: where the item is pressed; sub: for an item that opens a list
   beside it, where the pointer goes the frame after the press, the button
   still down: into the list, which takes the pointer's region) */
static void menu_at(int t, int i, int x, int sub)
{
    int tx = t_mid[t];
    fprintf(out, "/* menu %s, item %d */\n", titles[t], i);
    P(tx, 8, 0);
    f += 2;
    P(tx, 8, 1);
    /* the frame after the press the pointer is in the menu: Veranda (since
       its V81) opens a short menu within that frame and gives it the
       pointer's region, and a pointer left on the title then would be
       off its top: the menu would close (the edit menu, 2 items) */
    f += 1;
    P(m_left[t] + 10, BAR_H + 8, 1);
    f += 1;
    P(m_left[t] + 10, BAR_H + 8, 0);
    f += 10;
    P(x, BAR_H + i * IH + 8, 0);
    f += 4;
    P(x, BAR_H + i * IH + 8, 1);
    if (sub) {
        f += 1;
        P(sub, BAR_H + i * IH + 8, 1);
        f += 2;
        P(sub, BAR_H + i * IH + 8, 0);
    } else {
        f += 3;
        P(x, BAR_H + i * IH + 8, 0);
    }
    f += 70;                    /* the item's flash, then its routine */
    if (realmouse)
        f += RM_PAUSE;
}

static void type_digits(const char *d)
{
    for (; *d; ++d) {
        K(*d, "digit");
        f += 6;
    }
}

int main(int argc, char **argv)
{
    FILE *plan;
    char line[300], a[100];
    int n, g = 0, g2;
    char path[400];
    if (argc != 3 && argc != 4)
        die("usage: mkscript PLAN OUTDIR [STOP]");
    plan = fopen(argv[1], "r");
    dir = argv[2];
    if (argc == 4)
        stop = atoi(argv[3]);
    sprintf(path, "%s/script.bin", dir);
    bin = fopen(path, "wb");
    sprintf(path, "%s/script.h", dir);
    out = fopen(path, "w");
    sprintf(path, "%s/mouse.txt", dir);
    mouse = fopen(path, "w");
    if (!plan || !out || !mouse)
        die("cannot open the plan or the outputs");
    menu_geometry();
    P(320, 240, 0);
    f = 60;
    while (fgets(line, sizeof line, plan)) {
        if (line[0] == '#' || line[0] == '\n')
            continue;
        if (sscanf(line, "game %d %d", &g, &n) == 2) {
            sol_new((u8)g, (u32)n);
            layout_init();
            opt_autodrop = 0;
        } else if (sscanf(line, "auto %d", &n) == 1) {
            while (n-- > 0 && !won)
                if (!auto_one())
                    break;
        } else if (sscanf(line, "moves %99s", a) == 1) {
            FILE *m = fopen(a, "r");
            char ml[200], fs[16], ts[16];
            int cnt, drags = strstr(line + 6 + strlen(a), "drag") != 0;
            if (!m)
                die("no move list");
            while (fgets(ml, sizeof ml, m)) {
                if (sscanf(ml, "c %15s", fs) == 1 && stack_id(fs) >= 0) {
                    click_stack((u8)stack_id(fs));
                    continue;
                }
                if (sscanf(ml, "m %15s %15s %d", fs, ts, &cnt) != 3)
                    continue;
                if (!drags && stack_id(ts) >= st_f0 && stack_id(ts) < st_f0 + n_found && cnt == 1)
                    dbl((u8)stack_id(fs));
                else
                    drag((u8)stack_id(fs), (u8)cnt, (u8)stack_id(ts));
            }
            fclose(m);
        } else if (!strncmp(line, "undo", 4)) {
            K(0x80 | 'z', "Alt + Z: undo");
            f += 20;
            act_undo();
        } else if (!strncmp(line, "redo", 4)) {
            K(0x80 | 'y', "Alt + Y: redo");
            f += 20;
            act_redo();
        } else if (!strncmp(line, "state", 5)) {
            snapshot();
        } else if (sscanf(line, "menu %d %d", &n, &g2) == 2) {
            menu(n, g2);
            if (n == 1 && g2 == 1)
                sol_new(gid, deal_no);
            else if (n == 2)
                g2 ? act_redo() : act_undo();
            else if (n == 3 && g2 == 0)
                opt_autodrop ^= 1;
            else if (n == 3 && gid <= 1 && gid != g2 - 1)
                sol_new((u8)(g2 - 1), deal_no);
            g = gid;
            layout_init();
        } else if (sscanf(line, "choose %d", &n) == 1) {
            static const u8 list_game[9] = { 0, 2, 3, 4, 5, 6, 7, 8, 9 };
            /* Choose game pressed, the pointer then into its list, which
               opens beside it */
            menu_at(1, 3, m_left[1] + 10, c_left + 10);
            fprintf(out, "/* choose game %d */\n", n);
            P(c_left + 10, BAR_H + 3 * IH + n * IH + 8, 0);
            f += 4;
            P(c_left + 10, BAR_H + 3 * IH + n * IH + 8, 1);
            f += 3;
            P(c_left + 10, BAR_H + 3 * IH + n * IH + 8, 0);
            f += 60;
            g = list_game[n];
            sol_new((u8)g, 0);      /* a deal not known: a dealno must follow */
            layout_init();
        } else if (!strncmp(line, "ok", 2)) {
            fprintf(out, "/* OK */\n");
            click_at(140 + 19 * 8 + 20, 130 + 92 + 8);
            f += 30;
        } else if (sscanf(line, "dealno %d", &n) == 1) {
            sprintf(a, "%d", n);
            K(0x80 | 'd', "Alt + D: deal number");
            f += 40;
            type_digits(a);
            K(13, "Return");
            f += 60;
            sol_new((u8)g, (u32)n);
            layout_init();
        } else if (sscanf(line, "drag %15s %15s %d", a, a + 50, &n) == 3) {
            if (stack_id(a) < 0 || stack_id(a + 50) < 0)
                die(line);
            drag((u8)stack_id(a), (u8)n, (u8)stack_id(a + 50));
        } else if (sscanf(line, "shotnow %31s", a) == 1) {
            fprintf(out, "/* shot %s %d */\n", a, f);
        } else if (sscanf(line, "clickstack %15s", a) == 1) {
            if (stack_id(a) < 0)
                die(line);
            fprintf(out, "/* click %s */\n", a);
            click_stack((u8)stack_id(a));
        } else if (sscanf(line, "click %d %d", &n, &g2) == 2) {
            click_at(n, g2);
            f += 10;
        } else if (sscanf(line, "shot %31s", shot_name) == 1) {
            shot_next = 1;
            if (sscanf(line, "shot %*s %d", &n) == 1 && n >= 1 && n <= 6)
                shot_step = n;
        } else if (!strncmp(line, "realmouse", 9)) {
            /* from here on every press as the real-mouse script too
               (the pointer pushed to 0, 0 first); "end" exits it */
            realmouse = 1;
            rm_x = rm_y = rm_b = 0;
            fprintf(mouse, "0 id 3\n%d move -2000 -2000\n", 400 + f - 20);
        } else if (!strncmp(line, "mouse", 5)) {
            mouse_next = 1;
        } else if (sscanf(line, "dbl %15s %d", a, &g2) == 2) {
            /* a double click on stack a's top card, g2 frames between the
               first release and the second press */
            if (stack_id(a) < 0)
                die(line);
            dbl_gap = g2;
            dbl((u8)stack_id(a));
            dbl_gap = 4;
        } else if (sscanf(line, "wait %d", &n) == 1) {
            f += n;
        } else if (sscanf(line, "end %d", &n) == 1) {
            f += n;
            fprintf(out, "END(%d)\n", f);
            if (realmouse)
                fprintf(mouse, "%d exit\n", 400 + f);
            entry(0xFFFF, f > stop ? stop : f, 0);
        } else
            die(line);
    }
    fclose(out);
    fclose(mouse);
    fclose(bin);
    if (entries * 6 > 8191)
        die("the script is longer than a RAM bank");
    fprintf(stderr, "mkscript: %d frames, %d snapshots%s\n", f, nstate, won ? ", won" : "");
    return 0;
}
