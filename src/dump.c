/*
 * Veranda Solitaire: the state as text (the host tests and the test
 * build's STATE file use the same lines; tools/pysol_ref.py prints the
 * stacks the same way). GPL-3.0-or-later (see COPYING).
 */
#include "sol.h"
#include "dump.h"

static const char ranks[] = "A23456789TJQK";
static const char suitc[] = "CSHD";
static char *o;

static void str(const char *s)
{
    while (*s)
        *o++ = *s++;
}

static void num(u32 v)
{
    char d[11];
    u8 n = 0;
    do {
        d[n++] = (char)('0' + (u8)(v % 10));
        v /= 10;
    } while (v);
    while (n)
        *o++ = d[--n];
}

static void stack(const char *name, u8 idx, u8 s, u8 numbered)
{
    u8 i, c;
    str(name);
    if (numbered)
        num(idx);
    *o++ = ':';
    for (i = 0; i < s_n[s]; ++i) {
        c = s_c[s][i];
        *o++ = ' ';
        *o++ = (char)(ranks[RANK(c)] | (UP(c) ? 0 : 0x20));
        *o++ = (char)(suitc[SUIT(c)] | (UP(c) ? 0 : 0x20));
    }
    *o++ = '\n';
}

/* the state into buf; returns its length */
u16 sol_dump(char *buf, u8 header)
{
    u8 i;
    o = buf;
    if (header) {
        str("game ");
        num(gid);
        str(" deal ");
        num(deal_no);
        str(" round ");
        num(round_no);
        str(" moves ");
        num(moves);
        str(" won ");
        num(won);
        *o++ = '\n';
    }
    stack("talon", 0, st_talon, 0);
    if (st_waste != ANY && s_kind[st_waste] != K_GOLFWASTE)
        stack("waste", 0, st_waste, 0);
    for (i = 0; i < n_found; ++i)
        stack("found", i, st_f0 + i, 1);
    for (i = 0; i < n_rows; ++i)
        stack("row", i, st_r0 + i, 1);
    for (i = 0; i < n_res; ++i)
        stack("reserve", i, st_v0 + i, 1);
    *o = 0;
    return (u16)(o - buf);
}
