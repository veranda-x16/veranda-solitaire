/*
 * Veranda Solitaire: the engine (see sol.h). Each rule is PySolFC's,
 * translated from pysollib/stack.py and pysollib/game/__init__.py; the
 * comments name the Python that a part follows.
 *
 * Copyright (C) 2026 the Veranda Solitaire authors; derived from PySolFC
 * (see NOTICE). GPL-3.0-or-later (see COPYING).
 */
#include "sol.h"

const u8 rank_of[52] = {
    0,1,2,3,4,5,6,7,8,9,10,11,12, 0,1,2,3,4,5,6,7,8,9,10,11,12,
    0,1,2,3,4,5,6,7,8,9,10,11,12, 0,1,2,3,4,5,6,7,8,9,10,11,12 };
const u8 suit_of[52] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0, 1,1,1,1,1,1,1,1,1,1,1,1,1,
    2,2,2,2,2,2,2,2,2,2,2,2,2, 3,3,3,3,3,3,3,3,3,3,3,3,3 };

u8 gid;
const struct gdef *G;
u8 nstacks;
u8 s_kind[MAXSTACKS], s_n[MAXSTACKS], s_suit[MAXSTACKS], s_brank[MAXSTACKS];
s8 s_dir[MAXSTACKS];
u8 s_maxmove[MAXSTACKS], s_maxacc[MAXSTACKS], s_maxcards[MAXSTACKS], s_minacc[MAXSTACKS];
u8 s_seq[MAXSTACKS], s_rf[MAXSTACKS], s_cap[MAXSTACKS];
u8 s_block[MAXSTACKS][2];
u8 *s_c[MAXSTACKS];
u8 st_talon, st_waste, st_f0, st_r0, st_v0;
u8 n_found, n_rows, n_res;
u8 round_no;
u32 deal_no;
u16 moves;
u8 won;
u8 opt_autodrop;
u8 ncards;

static u8 pool[POOL];
static u8 logging;              /* atoms are logged (not during the deal) */
static u8 logged;               /* atoms logged by this action */

/*---------------------------------------------------------------
  LCRandom31 (pysol_cards/random.py): seedx = seedx * 214013 + 2531011,
  random() = bits 16-30 for a seed below 2^31; randint(a, b) =
  a + random() % (b + 1 - a). Bits 0-30 of the product depend only on
  bits 0-30 of seedx, so 31 bits are kept.
 ---------------------------------------------------------------*/
static u32 seedx;

void lc_seed(u32 seed)
{
    seedx = seed;
}

u16 lc_rand(void)
{
    seedx = (seedx * 214013UL + 2531011UL) & 0x7FFFFFFFUL;
    return (u16)((seedx >> 16) & 0x7FFF);
}

/*---------------------------------------------------------------
  atoms: applied, logged, undone
 ---------------------------------------------------------------*/
static void note(u8 s, u8 from)
{
    sol_changed(s, from);
}

static void put(u8 op, u8 a, u8 b, u8 n)
{
    if (logging) {
        log_put(op, a, b, n);
        logged = 1;
    }
}

static void at_move(u8 from, u8 to, u8 n)
{
    u8 *src = s_c[from] + s_n[from] - n;
    u8 *dst = s_c[to] + s_n[to];
    u8 i;
    note(from, s_n[from] - n);
    note(to, s_n[to]);
    for (i = 0; i < n; ++i)
        dst[i] = src[i];
    s_n[from] -= n;
    s_n[to] += n;
    put(A_MOVE, from, to, n);
}

static void at_flip(u8 s)
{
    s_c[s][s_n[s] - 1] ^= FACEUP;
    note(s, s_n[s] - 1);
    put(A_FLIP, s, 0, 0);
}

/* ATurnStackMove: every card of from onto to, one by one (so reversed),
   face down */
static void at_turn(u8 from, u8 to)
{
    u8 n = s_n[from];
    note(from, 0);
    note(to, s_n[to]);
    while (s_n[from]) {
        s_c[to][s_n[to]++] = s_c[from][--s_n[from]] & ~FACEUP;
    }
    put(A_TURN, from, to, n);
}

static void at_round(void)
{
    ++round_no;
    note(st_talon, s_n[st_talon]);
    put(A_ROUND, 0, 0, 0);
}

/* the inverse of one atom */
static void undo_atom(const u8 *a)
{
    u8 op = a[0] & 0x7F, x = a[1], y = a[2], n = a[3];
    switch (op) {
    case A_MOVE: {
        u8 *src = s_c[y] + s_n[y] - n;
        u8 *dst = s_c[x] + s_n[x];
        u8 i;
        note(y, s_n[y] - n);
        note(x, s_n[x]);
        for (i = 0; i < n; ++i)
            dst[i] = src[i];
        s_n[y] -= n;
        s_n[x] += n;
        break;
    }
    case A_FLIP:
        s_c[x][s_n[x] - 1] ^= FACEUP;
        note(x, s_n[x] - 1);
        break;
    case A_TURN:
        note(y, 0);
        note(x, s_n[x]);
        while (n--)
            s_c[x][s_n[x]++] = s_c[y][--s_n[y]] | FACEUP;
        break;
    case A_ROUND:
        --round_no;
        note(st_talon, s_n[st_talon]);
        break;
    }
}

static void redo_atom(const u8 *a)
{
    u8 op = a[0] & 0x7F;
    switch (op) {
    case A_MOVE: at_move(a[1], a[2], a[3]); break;
    case A_FLIP: at_flip(a[1]); break;
    case A_TURN: at_turn(a[1], a[2]); break;
    case A_ROUND: at_round(); break;
    }
}

/*---------------------------------------------------------------
  the rules
 ---------------------------------------------------------------*/
static u8 is_foundation(u8 s)
{
    return (u8)(s >= st_f0 && s < st_f0 + n_found);
}

/* Pyramid_RowStack.basicIsBlocked: a card below it is still there */
u8 sol_blocked(u8 s)
{
    u8 b;
    if (s_kind[s] != K_PYRROW)
        return 0;
    b = s_block[s][0];
    if (b != ANY && s_n[b])
        return 1;
    b = s_block[s][1];
    if (b != ANY && s_n[b])
        return 1;
    return 0;
}

static u8 free_reserves(void)
{
    u8 i, n = 0;
    for (i = 0; i < n_res; ++i)
        if (!s_n[st_v0 + i])
            ++n;
    return n;
}

static u8 free_rows(void)
{
    u8 i, n = 0;
    for (i = 0; i < n_rows; ++i)
        if (!s_n[st_r0 + i])
            ++n;
    return n;
}

/* is...Sequence(cards, mod=8192, dir): face up, building by dir */
static u8 is_seq(u8 seq, const u8 *c, u8 n, s8 dir)
{
    u8 i, a, b;
    for (i = 0; i < n; ++i)
        if (!UP(c[i]))
            return 0;
    for (i = 1; i < n; ++i) {
        a = c[i - 1];
        b = c[i];
        if ((int)RANK(a) + dir != (int)RANK(b))
            return 0;
        switch (seq) {
        case SEQ_AC: if (COLOR(a) == COLOR(b)) return 0; break;
        case SEQ_SC: if (COLOR(a) != COLOR(b)) return 0; break;
        case SEQ_SS: if (SUIT(a) != SUIT(b)) return 0; break;
        case SEQ_BO: if (SUIT(a) == SUIT(b)) return 0; break;
        }
    }
    return 1;
}

/* two cards of a sequence (the top card and the first one put on it) */
static u8 is_pair(u8 seq, u8 a, u8 b, s8 dir)
{
    u8 c[2];
    c[0] = a;
    c[1] = b;
    return is_seq(seq, c, 2, dir);
}

/* Stack.basicAcceptsCards */
static u8 basic_accepts(u8 to, u8 from, const u8 *c, u8 n)
{
    u8 i, s;
    if (from == to || sol_blocked(to))
        return 0;
    if (n < s_minacc[to] || n > s_maxacc[to])
        return 0;
    if ((u16)n + s_n[to] > s_maxcards[to])
        return 0;
    s = s_suit[to];
    for (i = 0; i < n; ++i)
        if (!UP(c[i]) || (s != ANY && SUIT(c[i]) != s))
            return 0;
    if (s_n[to])
        return (u8)(UP(TOP(to)) != 0);
    if (s != ANY && SUIT(c[0]) != s)
        return 0;
    if (s_brank[to] != ANY && RANK(c[0]) != s_brank[to])
        return 0;
    return 1;
}

/* Stack.basicCanMoveCards */
static u8 basic_can_move(u8 s, u8 n)
{
    u8 i;
    const u8 *c;
    if (sol_blocked(s))
        return 0;
    if (n < 1 || n > s_maxmove[s] || n > s_n[s])
        return 0;
    c = s_c[s] + s_n[s] - n;
    for (i = 0; i < n; ++i)
        if (!UP(c[i]))
            return 0;
    return 1;
}

/* SuperMoveStack_StackMethods._getMaxMove */
static u8 super_max(u8 s, u8 to_n)
{
    u8 mm = free_reserves() + 1;
    u8 e;
    if (s_brank[s] != ANY)
        return mm;
    e = free_rows();
    if (to_n == 0 && e)
        --e;
    while (e-- && mm < 128)
        mm <<= 1;
    return mm;
}

/* Pyramid_StackMethods.acceptsCards */
static u8 pyr_accepts(u8 to, u8 from, const u8 *c, u8 n)
{
    u8 t;
    if (sol_blocked(to))
        return 0;
    if (from == to || !s_n[to] || n != 1)
        return 0;
    t = TOP(to);
    return (u8)(UP(t) && UP(c[0]) && RANK(c[0]) + RANK(t) == 11);
}

u8 sol_accepts(u8 to, u8 from, u8 n)
{
    const u8 *c;
    u8 seq;
    if (n > s_n[from] || !n)
        return 0;
    c = s_c[from] + s_n[from] - n;
    switch (s_kind[to]) {
    case K_TALON:
        if (G->talon == T_FACEUP)
            return pyr_accepts(to, from, c, n);
        return 0;
    case K_ROW:     /* SequenceStack_StackMethods.acceptsCards */
        if (!basic_accepts(to, from, c, n))
            return 0;
        seq = (s_rf[to] & RF_SPIDER) ? SEQ_RK : s_seq[to];
        if (!is_seq(seq, c, n, s_dir[to]))
            return 0;
        if (s_n[to] && !is_pair(seq, TOP(to), c[0], s_dir[to]))
            return 0;
        if ((s_rf[to] & RF_SUPER) && n > super_max(to, s_n[to]))
            return 0;
        return 1;
    case K_YUKON:   /* Yukon_AC_RowStack.acceptsCards */
        if (!basic_accepts(to, from, c, n))
            return 0;
        if (s_n[to]) {
            u8 a = TOP(to), b = c[0];
            if ((int)RANK(a) + s_dir[to] != (int)RANK(b))
                return 0;
            if (s_seq[to] == SEQ_SS ? SUIT(a) != SUIT(b) : COLOR(a) == COLOR(b))
                return 0;
        }
        return 1;
    case K_FOUND:   /* SS_FoundationStack.acceptsCards */
        if (!basic_accepts(to, from, c, n))
            return 0;
        if (s_n[to] && (int)RANK(TOP(to)) + s_dir[to] != (int)RANK(c[0]))
            return 0;
        return 1;
    case K_SPIDERF: /* Spider_SS_Foundation.acceptsCards */
        if (!basic_accepts(to, from, c, n))
            return 0;
        return is_seq(SEQ_SS, c, n, s_dir[to]);
    case K_GOLFWASTE: { /* Golf_Waste.acceptsCards (strictness 0) */
        u8 r1, r2;
        if (from == st_talon)
            return 1;
        if (!basic_accepts(to, from, c, n) || !s_n[to])
            return 0;
        r1 = RANK(TOP(to));
        r2 = RANK(c[0]);
        return (u8)(r1 + 1 == r2 || r2 + 1 == r1);
    }
    case K_PYRROW:
    case K_PYRWASTE:
        return pyr_accepts(to, from, c, n);
    case K_PYRFOUND:    /* Pyramid_Foundation.acceptsCards */
        if (!basic_accepts(to, from, c, n))
            return 0;
        return (u8)(RANK(c[0]) == KING);
    default:        /* OpenStack: basicAcceptsCards (waste, reserves, Golf's rows) */
        return basic_accepts(to, from, c, n);
    }
}

u8 sol_can_move(u8 s, u8 n)
{
    const u8 *c;
    switch (s_kind[s]) {
    case K_TALON:   /* Stack.canMoveCards */
        return 0;
    case K_ROW:     /* SequenceStack_StackMethods.canMoveCards (+ SuperMove, FreeCell) */
        if (!basic_can_move(s, n))
            return 0;
        c = s_c[s] + s_n[s] - n;
        if (!is_seq(s_seq[s], c, n, s_dir[s]))
            return 0;
        if ((s_rf[s] & RF_SUPER) && n > super_max(s, 1))
            return 0;
        if ((s_rf[s] & RF_FREE) && n > free_reserves() + 1)
            return 0;
        return 1;
    default:        /* OpenStack.canMoveCards */
        return basic_can_move(s, n);
    }
}

/* OpenStack.canFlipCard (the talons: Stack's, no; FaceUpWasteTalonStack's) */
u8 sol_can_flip(u8 s)
{
    if (s_kind[s] == K_TALON && G->talon != T_FACEUP)
        return 0;
    if (sol_blocked(s) || !s_n[s])
        return 0;
    return (u8)(!UP(TOP(s)));
}

/* canDropCards: OpenStack's (the top card to a foundation), Spider's rows
   (spiderCanDropCards: the top 13), Pyramid_Talon's; foundations none */
u8 sol_can_drop(u8 s, u8 *to)
{
    u8 i, n = 1;
    switch (s_kind[s]) {
    case K_FOUND: case K_SPIDERF: case K_GOLFWASTE: case K_PYRFOUND:
        return 0;
    case K_TALON:
        if (G->talon != T_FACEUP || !s_n[s])
            return 0;
        break;
    case K_ROW:
        if (s_rf[s] & RF_SPIDER) {
            if (s_n[s] < 13)
                return 0;
            n = 13;
            break;
        }
        /* fall through */
    default:
        if (sol_blocked(s) || !s_n[s] || !sol_can_move(s, 1))
            return 0;
    }
    for (i = 0; i < n_found; ++i)
        if (st_f0 + i != s && sol_accepts(st_f0 + i, s, n)) {
            *to = st_f0 + i;
            return n;
        }
    return 0;
}

/* Game.isGameWon (Golf: the rows empty) */
u8 sol_is_won(void)
{
    u8 i, n = 0;
    if (G->flags & GF_WIN_ROWS) {
        for (i = 0; i < n_rows; ++i)
            if (s_n[st_r0 + i])
                return 0;
        return 1;
    }
    for (i = 0; i < n_found; ++i)
        n += s_n[st_f0 + i];
    return (u8)(n == ncards);
}

u8 sol_left(void)
{
    u8 i, n = 0;
    if (G->flags & GF_WIN_ROWS) {
        for (i = 0; i < n_rows; ++i)
            n += s_n[st_r0 + i];
        return n;
    }
    for (i = 0; i < n_found; ++i)
        n += s_n[st_f0 + i];
    return (u8)(ncards - n);
}

/* the talon's canDealCards (with the games' own conditions) */
u8 sol_can_deal(void)
{
    u8 t = st_talon, i;
    switch (G->talon) {
    case T_WASTE:
    case T_FACEUP:
        if ((G->flags & GF_WONDEAL) && sol_is_won())
            return 0;
        if (s_n[t])
            return 1;
        return (u8)(s_n[st_waste] && (!G->max_rounds || round_no != G->max_rounds));
    case T_DEALROW:
        if (!s_n[t])
            return 0;
        if (G->flags & GF_SPIDERDEAL)
            for (i = 0; i < n_rows; ++i)
                if (!s_n[st_r0 + i])
                    return 0;
        return 1;
    default:
        return (u8)(s_n[t] != 0);
    }
}

/* sol_has_move's state (static: small code on the 6502) */
static u8 hm_s, hm_n, hm_k;

/* some stack takes the top hm_n cards of hm_s (not a whole row onto an
   empty row: nothing changes) */
static u8 hm_takes(void)
{
    u8 t, whole = (u8)(hm_n == s_n[hm_s] && hm_s >= st_r0 && hm_s < st_r0 + n_rows);
    for (t = 0; t < nstacks; ++t)
        if (t != hm_s && sol_stack_is_open(t) && !(whole && !s_n[t] && t >= st_r0 && t < st_r0 + n_rows)
            && sol_accepts(t, hm_s, hm_n))
            return 1;
    return 0;
}

/* on the X16 these two run from the overlay's RAM bank (room in main RAM
   is short in the test build) */
#if defined(__CC65__) && defined(SOL_OVERLAY)
#pragma code-name (push, "OVERLAY1")
#endif

/* each card of stack s tried on the waste's top (in place of its top
   card, or on the waste when empty): some stack takes one */
static u8 hm_cards(u8 s)
{
    u8 w = st_waste, n = s_n[w], old, r = 0;
    if (!s_n[s])
        return 0;
    if (!n)
        s_n[w] = 1;     /* (the waste's top card, tried again at its turn: no harm) */
    old = s_c[w][s_n[w] - 1];
    hm_s = w;
    hm_n = 1;
    for (hm_k = 0; hm_k < s_n[s] && !r; ++hm_k) {
        s_c[w][s_n[w] - 1] = (u8)(s_c[s][hm_k] | FACEUP);
        r = hm_takes();
    }
    s_c[w][s_n[w] - 1] = old;
    s_n[w] = n;
    return r;
}

/* a move left to make (the status bar's "No moves left"): a card or a run
   from a stack the player takes from (sg.dropstacks, and a foundation
   that gives its top card back) onto one that takes it, a card to turn
   up, a deal. With a talon turned over without end (Klondike) dealing is
   no move unless a card of the talon or the waste would go somewhere:
   those cards come round again and again */
u8 sol_has_move(void)
{
    if (won)
        return 0;
    for (hm_s = 0; hm_s < nstacks; ++hm_s) {
        if (!s_n[hm_s] || !(sol_stack_is_drop(hm_s) || is_foundation(hm_s)))
            continue;       /* (a foundation's card back, where max_move lets it) */
        if (!is_foundation(hm_s) && sol_can_flip(hm_s))
            return 1;
        for (hm_n = 1; hm_n <= s_n[hm_s] && sol_can_move(hm_s, hm_n); ++hm_n)
            if (hm_takes())
                return 1;
    }
    if (!sol_can_deal())
        return 0;
    if (G->talon != T_WASTE || G->max_rounds || G->layout == L_GOLF)
        return 1;
    return (u8)(hm_cards(st_talon) || hm_cards(st_waste));
}

#if defined(__CC65__) && defined(SOL_OVERLAY)
#pragma code-name (pop)
#endif

/* FaceUpWasteTalonStack.fillStack: the talon's top face up */
static void fill_talon(void)
{
    if (G->talon == T_FACEUP && s_n[st_talon] && !UP(TOP(st_talon)))
        at_flip(st_talon);
}

/* one card from the talon (dealToStacks): flipped if flip */
static void deal_one(u8 to, u8 flip)
{
    if (flip)
        at_flip(st_talon);
    at_move(st_talon, to, 1);
}

/* the talon's dealCards */
static void talon_deal(void)
{
    u8 t = st_talon, w = st_waste, k, i;
    switch (G->talon) {
    case T_WASTE:
    case T_FACEUP:      /* WasteTalonStack.dealCards (+ FaceUpWasteTalonStack's) */
        if (s_n[t]) {
            k = G->num_deal;
            if (k > s_n[t])
                k = s_n[t];
            for (i = 0; i < k; ++i) {
                deal_one(w, !UP(TOP(t)));
                fill_talon();
            }
        } else if (s_n[w] && (!G->max_rounds || round_no != G->max_rounds)) {
            at_turn(w, t);
            at_round();
        }
        fill_talon();
        break;
    case T_DEALROW:     /* DealRowTalonStack.dealCards: dealRowAvail */
        k = n_rows;
        if (k > s_n[t])
            k = s_n[t];
        for (i = 0; i < k; ++i)
            deal_one(st_r0 + i, 1);
        break;
    }
}

/* sg.dropstacks: rows + reserves + waste (Golf: rows; Pyramid: + talon) */
u8 sol_stack_is_drop(u8 s)
{
    if (s >= st_r0 && s < st_r0 + n_rows)
        return 1;
    if (G->layout == L_GOLF)
        return 0;
    if (n_res && s >= st_v0 && s < st_v0 + n_res)
        return 1;
    if (s == st_waste)
        return 1;
    return (u8)((G->flags & GF_PAIRS) && s == st_talon);
}

/* sg.openstacks: where a dragged card may go (foundations, rows,
   reserves; Golf: the waste; Pyramid: + the talon and the waste) */
u8 sol_stack_is_open(u8 s)
{
    if (G->layout == L_GOLF)
        return (u8)(s == st_waste);
    if (s == st_talon)
        return (u8)((G->flags & GF_PAIRS) != 0);
    if (s == st_waste)
        return (u8)((G->flags & GF_PAIRS) != 0);
    return 1;
}

/* Game._autoPlay: flip, drop, deal until nothing changes (auto flip,
   PySolFC's autofaceup, and auto deal are on as PySolFC's defaults) */
static void autoplay(void)
{
    u8 done = 1, s, to, n, drop;
    drop = (u8)(opt_autodrop || (G->flags & GF_RUNS));
    while (done) {
        done = 0;
        if (won)
            return;
        if (!(G->flags & GF_NOAUTO)) {
            for (s = 0; s < nstacks; ++s)
                if (sol_stack_is_drop(s) && sol_can_flip(s)) {
                    at_flip(s);
                    done = 1;
                    if (sol_is_won()) {
                        won = 1;
                        return;
                    }
                }
            if (drop)
                for (s = 0; s < nstacks; ++s)
                    if (sol_stack_is_drop(s) && (n = sol_can_drop(s, &to)) != 0) {
                        at_move(s, to, n);
                        fill_talon();
                        done = 1;
                        if (sol_is_won()) {
                            won = 1;
                            return;
                        }
                    }
        }
        /* Game._autoDeal: a card to the waste when it is empty */
        if (st_waste != ANY && G->layout != L_GOLF && !s_n[st_waste] && sol_can_deal()) {
            talon_deal();
            done = 1;
            if (sol_is_won()) {
                won = 1;
                return;
            }
        }
    }
}

/* the end of a player's action: one undo step */
static u8 finish(void)
{
    if (!logged)
        return 0;
    log_mark_end(log_top - 1);
    log_cur = log_top;
    ++moves;
    logged = 0;
    if (sol_is_won())
        won = 1;
    return 1;
}

static void begin(void)
{
    /* a new move after undo forgets the moves undone */
    log_top = log_cur;
    logged = 0;
}

/* Pyramid_StackMethods.moveMove: to a foundation, or a pair to it */
static void pyr_move(u8 from, u8 to)
{
    u8 f = st_f0;
    if (to == f) {
        at_move(from, f, 1);
    } else {
        at_move(from, f, 1);
        at_move(to, f, 1);
    }
    fill_talon();
}

/* OpenStack.releaseHandler: the dragged cards dropped on to */
u8 act_move(u8 from, u8 to, u8 n)
{
    if (won || from == to || !sol_can_move(from, n) || !sol_accepts(to, from, n))
        return 0;
    begin();
    if (G->flags & GF_PAIRS)
        pyr_move(from, to);
    else
        at_move(from, to, n);
    if (sol_is_won())
        won = 1;
    else if (!is_foundation(from))      /* Stack.playMoveMove */
        autoplay();
    return finish();
}

/* Game.dealCards */
static u8 deal(void)
{
    if (!sol_can_deal())
        return 0;
    begin();
    talon_deal();
    if (sol_is_won())
        won = 1;
    else
        autoplay();
    return finish();
}

static u8 flip(u8 s)
{
    begin();
    at_flip(s);
    if (!sol_is_won())
        autoplay();
    return finish();
}

/* OpenStack.doubleclickHandler: flip, or the top card to a foundation */
u8 act_double(u8 s)
{
    u8 to, n;
    if (won)
        return 0;
    if (s == st_talon && G->talon != T_FACEUP)
        return deal();
    if (sol_stack_is_drop(s) && sol_can_flip(s))
        return flip(s);
    if (sol_stack_is_drop(s) && (n = sol_can_drop(s, &to)) != 0) {
        begin();
        if (G->flags & GF_PAIRS)
            pyr_move(s, to);
        else
            at_move(s, to, n);
        if (sol_is_won())
            won = 1;
        else
            autoplay();
        return finish();
    }
    return 0;
}

/* the click handlers: TalonStack (deal), Golf_RowStack (as a double
   click), the Pyramid stacks' _dropKingClickHandler, OpenStack (flip) */
u8 act_click(u8 s)
{
    if (won)
        return 0;
    if (G->flags & GF_PAIRS) {
        if (s_n[s] && UP(TOP(s)) && RANK(TOP(s)) == KING && !sol_blocked(s)
            && s_kind[s] != K_PYRFOUND) {
            begin();
            pyr_move(s, st_f0);
            if (sol_is_won())
                won = 1;
            else
                autoplay();
            return finish();
        }
    }
    if (s == st_talon)
        return deal();
    if (s_kind[s] == K_GOLFROW)
        return act_double(s);
    if (sol_stack_is_drop(s) && sol_can_flip(s))
        return flip(s);
    return 0;
}

u8 act_undo(void)
{
    u8 a[4];
    if (!log_cur)
        return 0;
    logging = 0;
    do {
        --log_cur;
        log_get(log_cur, a);
        undo_atom(a);
    } while (log_cur && !(log_get(log_cur - 1, a), a[0] & A_END));
    logging = 1;
    --moves;
    won = sol_is_won();
    return 1;
}

u8 act_redo(void)
{
    u8 a[4];
    if (log_cur >= log_top)
        return 0;
    logging = 0;
    do {
        log_get(log_cur, a);
        ++log_cur;
        redo_atom(a);
    } while (!(a[0] & A_END) && log_cur < log_top);
    logging = 1;
    ++moves;
    won = sol_is_won();
    return 1;
}

/*---------------------------------------------------------------
  a new game: the stacks (PySolFC's createGame), the cards
  (createCards, shuffle), the deal (startGame)
 ---------------------------------------------------------------*/
static u8 *pool_at;

/* on the X16 this part runs from a RAM bank (veranda/Makefile) */
#if defined(__CC65__) && defined(SOL_OVERLAY)
#pragma code-name (push, "OVERLAY1")
#endif

static u8 add_stack(u8 kind, u8 cap)
{
    u8 s = nstacks++;
    s_kind[s] = kind;
    s_n[s] = 0;
    s_suit[s] = ANY;
    s_brank[s] = ANY;
    s_dir[s] = 0;
    s_maxmove[s] = 1;           /* OpenStack: max_move=1, max_accept=0 */
    s_maxacc[s] = 0;
    s_maxcards[s] = 255;
    s_minacc[s] = 1;
    s_seq[s] = 0;
    s_rf[s] = 0;
    s_block[s][0] = s_block[s][1] = ANY;
    s_cap[s] = cap;
    s_c[s] = pool_at;
    pool_at += cap;
    return s;
}

static void foundation(u8 s, u8 suit)
{
    /* AbstractFoundationStack: suit, base_suit=suit, base_rank=ACE, dir=1,
       max_accept=1, max_cards=13 */
    s_suit[s] = suit;
    s_brank[s] = ACE;
    s_dir[s] = 1;
    s_maxacc[s] = 1;
    s_maxcards[s] = 13;
    s_maxmove[s] = G->fmaxmove;
}

void sol_new(u8 game, u32 deal)
{
    u8 i, j, k, n, s, d, suit;
    u8 cards[104];
    const u8 *p;

    gid = game;
    G = &games[game];
    deal_no = deal;
    nstacks = 0;
    pool_at = pool;
    ncards = (u8)(G->decks * 52);
    won = 0;
    moves = 0;
    round_no = 1;
    log_top = log_cur = 0;
    logging = 0;
    logged = 0;

    /* the stacks: talon, waste, foundations, rows, reserves */
    st_talon = add_stack(K_TALON, ncards);
    if (G->talon == T_FACEUP)
        s_maxacc[st_talon] = 1;
    st_waste = ANY;
    if (G->waste && G->waste != K_GOLFWASTE) {
        st_waste = add_stack(G->waste, ncards);
        if (G->waste == K_PYRWASTE)
            s_maxacc[st_waste] = 1;
    }
    n_found = G->nfound;
    st_f0 = nstacks;
    for (i = 0; i < n_found; ++i) {
        switch (G->fkind) {
        case K_FOUND:
            s = add_stack(K_FOUND, 13);
            foundation(s, (u8)(i % 4));     /* the layouts' suit=r.suit */
            break;
        case K_SPIDERF:         /* Spider_SS_Foundation */
            s = add_stack(K_SPIDERF, 13);
            s_dir[s] = -1;
            s_brank[s] = KING;
            s_minacc[s] = s_maxacc[s] = 13;
            s_maxmove[s] = 0;
            s_maxcards[s] = 13;
            break;
        case K_GOLFWASTE:       /* Golf_Waste: max_move=0, max_accept=1 */
            s = add_stack(K_GOLFWASTE, ncards);
            s_maxmove[s] = 0;
            s_maxacc[s] = 1;
            st_waste = s;
            break;
        case K_PYRFOUND:        /* suit=ANY_SUIT, dir=0, base_rank=ANY_RANK, max_move=0, max_cards=52 */
            s = add_stack(K_PYRFOUND, ncards);
            s_maxmove[s] = 0;
            s_maxacc[s] = 1;
            s_maxcards[s] = ncards;
            break;
        }
    }
    n_rows = G->nrows;
    st_r0 = nstacks;
    for (i = 0; i < n_rows; ++i) {
        s = add_stack(G->rkind, G->rkind == K_PYRROW ? 2 : ncards);
        s_dir[s] = -1;          /* BasicRowStack: dir=-1, base_rank=ANY_RANK */
        s_brank[s] = G->rbase;
        s_seq[s] = G->rseq;
        s_rf[s] = G->rflags;
        switch (G->rkind) {
        case K_ROW:             /* SequenceRowStack: max_move, max_accept unlimited */
        case K_YUKON:
            s_maxmove[s] = 255;
            s_maxacc[s] = 255;
            break;
        case K_PYRROW:          /* max_accept=1, max_cards=2 */
            s_maxacc[s] = 1;
            s_maxcards[s] = 2;
            s_dir[s] = 0;
            break;
        }
    }
    if (G->rkind == K_PYRROW) {     /* Pyramid._createPyramid's blockmap */
        n = 0;
        for (i = 0; i < 6; ++i)
            for (j = 0; j <= i; ++j) {
                k = (u8)(n + i + 1);
                s_block[st_r0 + n][0] = st_r0 + k;
                s_block[st_r0 + n][1] = st_r0 + k + 1;
                ++n;
            }
    }
    n_res = G->nres;
    st_v0 = nstacks;
    for (i = 0; i < n_res; ++i) {
        s = add_stack(K_RESERVE, 1);    /* ReserveStack: max_accept=1, max_cards=1 */
        s_maxacc[s] = 1;
        s_maxcards[s] = 1;
    }

    /* Game.createCards: for each deck, each suit, each rank */
    n = 0;
    for (d = 0; d < G->decks; ++d)
        for (i = 0; i < 4; ++i) {
            suit = G->suits[i];
            for (j = 0; j < 13; ++j)
                cards[n++] = (u8)(suit * 13 + j);
        }
    /* Game.shuffle: ms_rearrange for 52 cards, then RandomBase.shuffle */
    p = cards;
    if (ncards == 52) {
        u8 tmp[52];
        static const u8 off[4] = { 0, 39, 26, 13 };
        for (i = 0; i < 52; ++i)
            tmp[i] = cards[i];
        n = 0;
        for (i = 0; i < 13; ++i)
            for (j = 0; j < 4; ++j)
                cards[n++] = tmp[i + off[j]];
    }
    lc_seed(deal);
    for (i = (u8)(ncards - 1); i > 0; --i) {
        j = (u8)(lc_rand() % (u16)(i + 1));
        k = cards[i];
        cards[i] = cards[j];
        cards[j] = k;
    }
    for (i = 0; i < ncards; ++i)
        s_c[st_talon][i] = cards[i];        /* face down; the last is the top */
    s_n[st_talon] = ncards;

    /* startGame: the deal program */
    for (p = G->deal; *p != D_END;) {
        switch (*p) {
        case D_ROWS: {
            u8 a = p[1], b = p[2], f = p[3];
            if (f & DF_REV)
                for (i = b + 1; i-- > a;)
                    deal_one(st_r0 + i, f & DF_FLIP);
            else
                for (i = a; i <= b; ++i)
                    deal_one(st_r0 + i, f & DF_FLIP);
            p += 4;
            break;
        }
        case D_LIST:
            n = p[1];
            for (i = 0; i < n; ++i)
                deal_one(st_r0 + p[3 + i], p[2] & DF_FLIP);
            p += 3 + n;
            break;
        case D_CARDS:
            talon_deal();
            ++p;
            break;
        }
    }
    for (s = 0; s < nstacks; ++s)
        note(s, 0);
    logging = 1;
}

#if defined(__CC65__) && defined(SOL_OVERLAY)
#pragma code-name (pop)
#endif
