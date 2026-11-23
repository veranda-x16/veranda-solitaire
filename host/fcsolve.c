/*
 * A small FreeCell solver (host only): finds a move list for a deal with
 * single-card moves, in the engine's notation (m FROM TO 1), so that the
 * host tests and the emulator's scripted session can play it.
 *
 *   fcsolve N > freecellN.moves
 *
 * Best-first search on a simple score (cards home, cards buried under
 * higher ones, free cells used), positions seen kept in a hash table;
 * after each move the cards that are safe to send home go home (a card
 * no lower card of the other colour can still need). Uses the engine
 * only for the deal (sol_new). GPL-3.0-or-later (see COPYING).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "host.h"

typedef struct {
    u8 col[8][20], n[8];
    u8 cell[4];          /* 0xFF empty */
    u8 found[4];         /* cards on each suit's foundation */
} pos;

typedef struct {
    pos p;
    int parent;
    u8 nm;               /* moves from the parent */
    u8 mv[24][2];        /* from, to: 0-7 columns, 8-11 cells, 12 home */
    int score;
} node;

#define MAXN 1500000
static node *nodes;
static int nn;
static int *heap, hn;

#define HBITS 23
static unsigned long long *seen;

static int rnk(u8 c) { return c % 13; }
static int sut(u8 c) { return c / 13; }
static int red(u8 c) { return sut(c) >= 2; }

static unsigned long long hash_pos(const pos *P)
{
    unsigned long long h = 1469598103934665603ULL, hc[8], t;
    int i, j, k;
    u8 cells[4];
    for (i = 0; i < 8; ++i) {
        t = 1099511628211ULL + P->n[i];
        for (j = 0; j < P->n[i]; ++j)
            t = (t ^ (P->col[i][j] + 1)) * 1099511628211ULL;
        hc[i] = t;
    }
    for (i = 1; i < 8; ++i)
        for (j = i; j > 0 && hc[j - 1] > hc[j]; --j) {
            t = hc[j]; hc[j] = hc[j - 1]; hc[j - 1] = t;
        }
    memcpy(cells, P->cell, 4);
    for (i = 1; i < 4; ++i)
        for (k = i; k > 0 && cells[k - 1] > cells[k]; --k) {
            u8 x = cells[k]; cells[k] = cells[k - 1]; cells[k - 1] = x;
        }
    for (i = 0; i < 8; ++i)
        h = (h ^ hc[i]) * 1099511628211ULL;
    for (i = 0; i < 4; ++i)
        h = (h ^ cells[i]) * 1099511628211ULL;
    return h | 1;
}

static int visit(const pos *P)
{
    unsigned long long h = hash_pos(P);
    unsigned long i = (unsigned long)(h >> 20) & ((1UL << HBITS) - 1);
    while (seen[i]) {
        if (seen[i] == h)
            return 0;
        i = (i + 1) & ((1UL << HBITS) - 1);
    }
    seen[i] = h;
    return 1;
}

static int home(const pos *P)
{
    return P->found[0] + P->found[1] + P->found[2] + P->found[3];
}

static int score(const pos *P)
{
    int s = (52 - home(P)) * 10, i, j, k, free = 0;
    for (i = 0; i < 4; ++i)
        if (P->cell[i] != 0xFF)
            s += 4;
    for (i = 0; i < 8; ++i) {
        if (!P->n[i])
            free++;
        /* cards above a lower card of the same suit */
        for (j = 0; j < P->n[i]; ++j)
            for (k = j + 1; k < P->n[i]; ++k)
                if (sut(P->col[i][k]) == sut(P->col[i][j]) && rnk(P->col[i][k]) > rnk(P->col[i][j])) {
                    s += 3;
                    break;
                }
        /* a column out of sequence */
        for (j = P->n[i] - 1; j > 0; --j) {
            u8 a = P->col[i][j - 1], b = P->col[i][j];
            if (!(rnk(a) == rnk(b) + 1 && red(a) != red(b)))
                break;
        }
        s += j;
    }
    return s - free * 3;
}

static int top(const pos *P, int from, u8 *c)
{
    if (from < 8) {
        if (!P->n[from])
            return 0;
        *c = P->col[from][P->n[from] - 1];
        return 1;
    }
    if (P->cell[from - 8] == 0xFF)
        return 0;
    *c = P->cell[from - 8];
    return 1;
}

static void apply(pos *P, int from, int to)
{
    u8 c;
    if (from < 8)
        c = P->col[from][--P->n[from]];
    else {
        c = P->cell[from - 8];
        P->cell[from - 8] = 0xFF;
    }
    if (to < 8)
        P->col[to][P->n[to]++] = c;
    else if (to < 12)
        P->cell[to - 8] = c;
    else
        ++P->found[sut(c)];
}

/* safe to go home: the cards of the other colour one lower are home */
static int safe(const pos *P, u8 c)
{
    int r = rnk(c), o1, o2;
    if (P->found[sut(c)] != r)
        return 0;
    if (r <= 1)
        return 1;
    o1 = red(c) ? 0 : 2;
    o2 = o1 + 1;
    return P->found[o1] >= r - 1 && P->found[o2] >= r - 1;
}

static void heap_push(int i)
{
    int k = hn++, p;
    heap[k] = i;
    while (k) {
        p = (k - 1) / 2;
        if (nodes[heap[p]].score <= nodes[heap[k]].score)
            break;
        int t = heap[p]; heap[p] = heap[k]; heap[k] = t;
        k = p;
    }
}

static int heap_pop(void)
{
    int r = heap[0], k = 0, c;
    heap[0] = heap[--hn];
    for (;;) {
        c = 2 * k + 1;
        if (c >= hn)
            break;
        if (c + 1 < hn && nodes[heap[c + 1]].score < nodes[heap[c]].score)
            ++c;
        if (nodes[heap[k]].score <= nodes[heap[c]].score)
            break;
        int t = heap[c]; heap[c] = heap[k]; heap[k] = t;
        k = c;
    }
    return r;
}

static int depth_of(int i)
{
    int d = 0;
    while (nodes[i].parent >= 0) {
        d += nodes[i].nm;
        i = nodes[i].parent;
    }
    return d;
}

/* the child of node par by the move from, to (and the safe moves after) */
static int child(int par, int from, int to)
{
    node *n;
    int f, moved;
    u8 c;
    if (nn >= MAXN)
        return -1;
    n = &nodes[nn];
    n->p = nodes[par].p;
    n->parent = par;
    n->nm = 0;
    apply(&n->p, from, to);
    n->mv[n->nm][0] = (u8)from;
    n->mv[n->nm][1] = (u8)to;
    n->nm++;
    do {
        moved = 0;
        for (f = 0; f < 12; ++f)
            if (top(&n->p, f, &c) && safe(&n->p, c) && n->nm < 24) {
                apply(&n->p, f, 12);
                n->mv[n->nm][0] = (u8)f;
                n->mv[n->nm][1] = 12;
                n->nm++;
                moved = 1;
            }
    } while (moved);
    if (!visit(&n->p))
        return -1;
    n->score = score(&n->p) + depth_of(par) / 4;
    return nn++;
}

static void print(int i)
{
    static int path[2000];
    int k = 0, j, m;
    u8 c;
    pos P;
    while (nodes[i].parent >= 0) {
        path[k++] = i;
        i = nodes[i].parent;
    }
    P = nodes[i].p;
    m = 0;
    for (j = k - 1; j >= 0; --j)
        m += nodes[path[j]].nm;
    printf("# FreeCell deal: a solution found by host/fcsolve.c (%d moves)\n", m);
    for (j = k - 1; j >= 0; --j) {
        node *n = &nodes[path[j]];
        for (m = 0; m < n->nm; ++m) {
            int from = n->mv[m][0], to = n->mv[m][1];
            top(&P, from, &c);
            if (from < 8)
                printf("m r%d ", from);
            else
                printf("m v%d ", from - 8);
            if (to < 8)
                printf("r%d 1\n", to);
            else if (to < 12)
                printf("v%d 1\n", to - 8);
            else
                printf("f%d 1\n", sut(c));
            apply(&P, from, to);
        }
    }
}

int main(int argc, char **argv)
{
    int i, j, f, t, cur;
    u8 c, d;
    u32 deal = argc > 1 ? (u32)strtoul(argv[1], 0, 10) : 1;
    pos *P;
    sol_new(2, deal);           /* FreeCell */
    nodes = malloc(sizeof(node) * MAXN);
    heap = malloc(sizeof(int) * MAXN);
    seen = calloc(1UL << HBITS, sizeof *seen);
    P = &nodes[0].p;
    memset(P, 0, sizeof *P);
    for (i = 0; i < 8; ++i) {
        P->n[i] = s_n[st_r0 + i];
        for (j = 0; j < P->n[i]; ++j)
            P->col[i][j] = CARDID(s_c[st_r0 + i][j]);
    }
    for (i = 0; i < 4; ++i)
        P->cell[i] = 0xFF;
    nodes[0].parent = -1;
    nodes[0].nm = 0;
    nn = 1;
    visit(P);
    heap_push(0);
    while (hn) {
        cur = heap_pop();
        P = &nodes[cur].p;
        if (home(P) == 52) {
            print(cur);
            return 0;
        }
        for (f = 0; f < 12; ++f) {
            if (!top(&nodes[cur].p, f, &c))
                continue;
            if (nodes[cur].p.found[sut(c)] == rnk(c) && (i = child(cur, f, 12)) >= 0)
                heap_push(i);
            for (t = 0; t < 8; ++t) {
                P = &nodes[cur].p;
                if (t == f)
                    continue;
                if (P->n[t]) {
                    d = P->col[t][P->n[t] - 1];
                    if (!(rnk(d) == rnk(c) + 1 && red(d) != red(c)))
                        continue;
                } else if (f < 8 && P->n[f] == 1)
                    continue;
                if (P->n[t] >= 19)
                    continue;
                if ((i = child(cur, f, t)) >= 0)
                    heap_push(i);
            }
            if (f < 8)
                for (t = 0; t < 4; ++t)
                    if (nodes[cur].p.cell[t] == 0xFF) {
                        if ((i = child(cur, f, 8 + t)) >= 0)
                            heap_push(i);
                        break;
                    }
        }
    }
    fprintf(stderr, "fcsolve: no solution for deal %lu\n", (unsigned long)deal);
    return 1;
}
