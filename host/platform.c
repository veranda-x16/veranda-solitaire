/*
 * Veranda Solitaire on the host (gcc): the engine's platform part, the
 * move log in an array, and the helpers the host tools share: stack
 * names, moves read from text.
 * GPL-3.0-or-later (see COPYING).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "host.h"

#define LOGMAX 60000
static u8 lg[LOGMAX][4];
u16 log_top, log_cur;

u8 log_put(u8 op, u8 a, u8 b, u8 n)
{
    if (log_top >= LOGMAX)
        return 0;
    lg[log_top][0] = op;
    lg[log_top][1] = a;
    lg[log_top][2] = b;
    lg[log_top][3] = n;
    ++log_top;
    return 1;
}

void log_get(u16 i, u8 *a)
{
    memcpy(a, lg[i], 4);
}

void log_mark_end(u16 i)
{
    lg[i][0] |= A_END;
}

u8 changed_from[MAXSTACKS];

void sol_changed(u8 s, u8 from)
{
    if (from < changed_from[s])
        changed_from[s] = from;
}

/* t, w, f<i>, r<i>, v<i> */
int stack_id(const char *n)
{
    int i = atoi(n + 1);
    switch (n[0]) {
    case 't': return st_talon;
    case 'w': return st_waste == ANY ? -1 : st_waste;
    case 'f': return i < n_found ? st_f0 + i : -1;
    case 'r': return i < n_rows ? st_r0 + i : -1;
    case 'v': return i < n_res ? st_v0 + i : -1;
    }
    return -1;
}

const char *stack_name(u8 s)
{
    static char bb[4][8];
    static u8 k;
    char *b = bb[k++ & 3];
    if (s == st_talon)
        return "t";
    if (s == st_waste && s_kind[s] != K_GOLFWASTE)
        return "w";
    if (s >= st_f0 && s < st_f0 + n_found)
        sprintf(b, "f%d", s - st_f0);
    else if (s >= st_r0 && s < st_r0 + n_rows)
        sprintf(b, "r%d", s - st_r0);
    else
        sprintf(b, "v%d", s - st_v0);
    return b;
}

/* one line of a move list: m FROM TO N | c S | d S | u | r | a 0/1;
   the result of the action (1 done), -1 a bad line, 2 nothing (a comment) */
int do_line(const char *line, struct act *out)
{
    char a[16], b[16];
    int n = 0, f, t;
    struct act dummy;
    if (!out)
        out = &dummy;
    memset(out, 0, sizeof *out);
    while (*line == ' ')
        ++line;
    if (!*line || *line == '#' || *line == '\n')
        return 2;
    out->op = line[0];
    switch (line[0]) {
    case 'm':
        if (sscanf(line + 1, "%15s %15s %d", a, b, &n) != 3)
            return -1;
        f = stack_id(a);
        t = stack_id(b);
        if (f < 0 || t < 0)
            return -1;
        out->a = (u8)f;
        out->b = (u8)t;
        out->n = (u8)n;
        return act_move((u8)f, (u8)t, (u8)n);
    case 'c':
    case 'd':
        if (sscanf(line + 1, "%15s", a) != 1 || (f = stack_id(a)) < 0)
            return -1;
        out->a = (u8)f;
        return line[0] == 'c' ? act_click((u8)f) : act_double((u8)f);
    case 'u':
        return act_undo();
    case 'r':
        return act_redo();
    case 'a':
        opt_autodrop = (u8)atoi(line + 1);
        out->n = opt_autodrop;
        return 1;
    }
    return -1;
}

void print_state(FILE *f, int header)
{
    static char buf[4096];
    sol_dump(buf, (u8)header);
    fputs(buf, f);
}
