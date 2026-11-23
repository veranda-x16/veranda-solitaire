/*
 * A small Klondike solver (host only), over the engine itself: depth
 * first, the moves the engine allows (home, uncovering, the waste, the
 * talon, the rest), positions seen kept by a hash of the state's text,
 * PySolFC's autoplay as the engine does it (auto flip, auto deal; auto
 * drop off). It writes the moves in the engine's notation:
 *
 *   klsolve GAME DEAL [MAXNODES] > moves     (GAME 0: draw 1, 1: draw 3)
 *
 * GPL-3.0-or-later (see COPYING).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "host.h"

#define HBITS 24
static unsigned long long *seen;
static long nodes, maxnodes = 3000000;
static char path[3000][24];
static int depth;

static unsigned long long hash_state(void)
{
    static char buf[4096];
    unsigned long long h = 1469598103934665603ULL;
    char *p;
    sol_dump(buf, 0);
    for (p = buf; *p; ++p)
        h = (h ^ (unsigned char)*p) * 1099511628211ULL;
    return h | 1;
}

static int visit(void)
{
    unsigned long long h = hash_state();
    unsigned long i = (unsigned long)(h >> 20) & ((1UL << HBITS) - 1);
    while (seen[i]) {
        if (seen[i] == h)
            return 0;
        i = (i + 1) & ((1UL << HBITS) - 1);
    }
    seen[i] = h;
    return 1;
}

static int solve(void);

static int try_line(const char *line)
{
    if (do_line(line, 0) != 1)
        return 0;
    if (++nodes > maxnodes) {
        act_undo();
        return 0;
    }
    if (!visit()) {
        act_undo();
        return 0;
    }
    strcpy(path[depth++], line);
    if (solve())
        return 1;
    --depth;
    act_undo();
    return 0;
}

static int solve(void)
{
    char line[40];
    u8 s, t, to, i, n, d;
    if (won)
        return 1;
    if (depth >= 2900 || nodes > maxnodes)
        return 0;
    /* home */
    for (s = 0; s < nstacks; ++s)
        if (sol_stack_is_drop(s) && sol_can_drop(s, &to)) {
            sprintf(line, "m %s %s 1", stack_name(s), stack_name(to));
            if (try_line(line))
                return 1;
        }
    /* runs that uncover a card, or bring a king to an empty row only
       from under a card */
    for (s = st_r0; s < st_r0 + n_rows; ++s) {
        for (i = 0; i < s_n[s] && !UP(s_c[s][i]); ++i)
            ;
        if (i >= s_n[s])
            continue;
        for (d = i; d < s_n[s]; ++d) {
            n = s_n[s] - d;
            if (!sol_can_move(s, n))
                continue;
            for (t = st_r0; t < st_r0 + n_rows; ++t) {
                if (t == s || !sol_accepts(t, s, n))
                    continue;
                if (!s_n[t] && d == 0)
                    continue;           /* a king from an empty row to another */
                if (d != i && s_n[t] && RANK(s_c[s][d - 1]) == RANK(TOP(t)) && COLOR(s_c[s][d - 1]) == COLOR(TOP(t)))
                    continue;           /* the same place again */
                sprintf(line, "m %s %s %d", stack_name(s), stack_name(t), n);
                if (try_line(line))
                    return 1;
            }
        }
    }
    /* the waste to a row */
    if (s_n[st_waste])
        for (t = st_r0; t < st_r0 + n_rows; ++t)
            if (sol_accepts(t, st_waste, 1)) {
                sprintf(line, "m w %s 1", stack_name(t));
                if (try_line(line))
                    return 1;
            }
    /* a foundation's card back down */
    for (s = st_f0; s < st_f0 + n_found; ++s)
        for (t = st_r0; t < st_r0 + n_rows; ++t)
            if (s_n[s] && RANK(TOP(s)) > 1 && sol_accepts(t, s, 1)) {
                sprintf(line, "m %s %s 1", stack_name(s), stack_name(t));
                if (try_line(line))
                    return 1;
            }
    /* the talon */
    if (sol_can_deal() && try_line("c t"))
        return 1;
    return 0;
}

int main(int argc, char **argv)
{
    int g = argc > 1 ? atoi(argv[1]) : 0, i;
    u32 deal = argc > 2 ? (u32)strtoul(argv[2], 0, 10) : 1;
    if (argc > 3)
        maxnodes = atol(argv[3]);
    sol_new((u8)g, deal);
    opt_autodrop = 0;
    seen = calloc(1UL << HBITS, sizeof *seen);
    visit();
    if (!solve()) {
        fprintf(stderr, "klsolve: no solution for game %d deal %lu (%ld positions)\n", g, (unsigned long)deal, nodes);
        return 1;
    }
    printf("# %s #%lu: a solution found by host/klsolve.c (%d moves, %ld positions)\n",
           games[g].name, (unsigned long)deal, depth, nodes);
    for (i = 0; i < depth; ++i)
        printf("%s\n", path[i]);
    return 0;
}
