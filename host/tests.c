/*
 * Veranda Solitaire: the engine's host tests. Each game's rules on
 * chosen positions, against what PySolFC's classes do (the comments name
 * them); deals reproduced; undo and redo; win detection; FreeCell deal
 * #1 and Klondike deal #1 solved by move lists (host/fcsolve.c's and
 * host/klsolve.c's).
 *
 *   tests [freecell1.moves [klondike1.moves]]
 *
 * GPL-3.0-or-later (see COPYING).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "host.h"

static int fails, checks;
static const char *suite;

#define CHECK(cond, what) check((cond) != 0, what, __LINE__)
static void check(int ok, const char *what, int line)
{
    ++checks;
    if (!ok) {
        ++fails;
        printf("FAIL  %s: %s (line %d)\n", suite, what, line);
    }
}

/* a card from text: rank A23456789TJQK, suit CSHD; lower case face down */
static u8 card(const char *t)
{
    static const char R[] = "A23456789TJQK", S[] = "CSHD";
    char r = t[0], s = t[1];
    int up = !(r >= 'a' && r <= 'z') && !(s >= 'a' && s <= 'z');
    if (r >= 'a') r -= 32;
    if (s >= 'a') s -= 32;
    return (u8)((strchr(S, s) - S) * 13 + (strchr(R, r) - R)) | (up ? FACEUP : 0);
}

/* stack s holds the cards of text (bottom first) */
static void set(const char *name, const char *text)
{
    int s = stack_id(name);
    u8 n = 0;
    if (s < 0) {
        printf("FAIL  %s: no stack %s\n", suite, name);
        exit(1);
    }
    while (*text) {
        while (*text == ' ')
            ++text;
        if (!*text)
            break;
        s_c[s][n++] = card(text);
        text += 2;
    }
    s_n[s] = n;
}

/* a game with every stack empty, to set a position up */
static void empty(u8 g)
{
    u8 s;
    sol_new(g, 1);
    for (s = 0; s < nstacks; ++s)
        s_n[s] = 0;
    opt_autodrop = 0;
}

static int S(const char *n) { return stack_id(n); }

static char *text(const char *name)
{
    static char b[400];
    char *o = b;
    int s = stack_id(name), i;
    static const char R[] = "A23456789TJQK", SU[] = "CSHD";
    for (i = 0; i < s_n[s]; ++i) {
        u8 c = s_c[s][i];
        if (i)
            *o++ = ' ';
        *o++ = (char)(R[RANK(c)] | (UP(c) ? 0 : 32));
        *o++ = (char)(SU[SUIT(c)] | (UP(c) ? 0 : 32));
    }
    *o = 0;
    return b;
}

static void klondike(void)
{
    u8 to;
    suite = "Klondike";
    empty(0);
    set("r0", "");
    set("r1", "qh KS");
    set("r2", "QD");
    CHECK(sol_accepts(S("r0"), S("r1"), 1), "an empty row takes a king (KingAC_RowStack, base_rank=KING)");
    CHECK(!sol_accepts(S("r0"), S("r2"), 1), "an empty row refuses a queen");
    set("r3", "9S");
    set("r4", "8H");
    set("r5", "8C");
    set("r6", "7H");
    CHECK(sol_accepts(S("r3"), S("r4"), 1), "red 8 on black 9");
    CHECK(!sol_accepts(S("r3"), S("r5"), 1), "black 8 on black 9 refused (isAlternateColorSequence)");
    CHECK(!sol_accepts(S("r3"), S("r6"), 1), "7 on 9 refused");
    set("r4", "jd 8H 7C");
    CHECK(sol_can_move(S("r4"), 2), "8H 7C moves as a sequence");
    CHECK(sol_accepts(S("r3"), S("r4"), 2), "8H 7C onto 9S");
    CHECK(!sol_can_move(S("r4"), 3), "a face-down card does not move (basicCanMoveCards)");
    set("r4", "8H 7D");
    CHECK(!sol_can_move(S("r4"), 2), "8H 7D is not a sequence");
    /* foundations: f0 clubs, f1 spades, f2 hearts, f3 diamonds */
    set("r5", "AC");
    set("r6", "AD");
    CHECK(sol_accepts(S("f0"), S("r5"), 1), "the clubs' foundation takes the ace of clubs");
    CHECK(!sol_accepts(S("f0"), S("r6"), 1), "and not the ace of diamonds (SS_FoundationStack suit=)");
    CHECK(!sol_accepts(S("f2"), S("r5"), 1), "the hearts' foundation refuses clubs");
    set("r6", "2C");
    CHECK(!sol_accepts(S("f0"), S("r6"), 1), "an empty foundation refuses a 2 (base_rank=ACE)");
    set("f0", "AC");
    CHECK(sol_accepts(S("f0"), S("r6"), 1), "2C on AC");
    set("r6", "jh 3C");
    CHECK(!sol_accepts(S("f0"), S("r6"), 1), "3C on AC refused");
    /* a foundation's card may come back (max_move=1) */
    set("f0", "AC 2C 3C");
    set("r1", "4D");
    CHECK(sol_can_move(S("f0"), 1) && sol_accepts(S("r1"), S("f0"), 1), "3C back from the foundation onto 4D");
    CHECK(!sol_can_move(S("f0"), 2), "only one card from a foundation");
    /* the waste: takes nothing, gives its top card */
    set("w", "5H 6S");
    CHECK(!sol_accepts(S("w"), S("r1"), 1), "the waste takes nothing (max_accept=0)");
    CHECK(sol_can_move(S("w"), 1) && !sol_can_move(S("w"), 2), "the waste gives one card");
    /* the talon (WasteTalonStack, num_deal=1, max_rounds=-1) */
    empty(0);
    set("t", "ks qd 2h");
    set("w", "");
    CHECK(act_click(S("t")) && !strcmp(text("w"), "2H") && !strcmp(text("t"), "ks qd"), "a click deals one card face up");
    set("t", "");
    set("w", "3H 4S 5D");
    round_no = 1;
    CHECK(act_click(S("t")) && s_n[S("t")] == 2, "an empty talon turns the waste over (ATurnStackMove)");
    CHECK(round_no == 2, "the next round");
    /* auto deal: the waste emptied gets the talon's next card */
    CHECK(!strcmp(text("w"), "3H") && !strcmp(text("t"), "5d 4s"), "and the waste, empty, gets a card (autodeal)");
    /* draw 3 */
    empty(1);
    set("t", "ks qd 2h 3c 4d");
    CHECK(act_click(S("t")) && !strcmp(text("w"), "4D 3C 2H") && !strcmp(text("t"), "ks qd"), "draw 3: three cards, the talon's top first");
    CHECK(act_click(S("t")) && !strcmp(text("w"), "4D 3C 2H QD KS"), "then the last two");
    /* auto flip after a move, and the double click */
    empty(0);
    set("r0", "kc 2D");
    set("r1", "3S");
    set("t", "5c");
    CHECK(act_move(S("r0"), S("r1"), 1), "2D onto 3S");
    CHECK(!strcmp(text("r0"), "KC"), "the king under it turned face up (autofaceup)");
    set("w", "AC");
    CHECK(sol_can_drop(S("w"), &to) == 1 && to == S("f0"), "the waste's ace can drop on the clubs");
    CHECK(act_double(S("w")) && !strcmp(text("f0"), "AC"), "a double click sends it there");
    /* auto drop (options) */
    empty(0);
    set("r0", "AH");
    set("r1", "2H");
    set("r2", "kc");
    set("r3", "QD");
    set("t", "5c 6c");
    opt_autodrop = 1;
    CHECK(act_move(S("r3"), S("r2"), 0) == 0, "no move of nothing");
    set("r2", "KC");
    CHECK(act_move(S("r3"), S("r2"), 1), "QD onto KC");
    CHECK(!strcmp(text("f2"), "AH 2H"), "with auto drop the aces and twos go home");
    opt_autodrop = 0;
    /* undo of that move puts everything back */
    CHECK(act_undo() && !strcmp(text("r3"), "QD") && !strcmp(text("r0"), "AH") && !s_n[S("f2")], "undo puts the move and its drops back");
    CHECK(act_redo() && !strcmp(text("f2"), "AH 2H") && !strcmp(text("r2"), "KC QD"), "redo does them again");
    /* win */
    empty(0);
    set("f0", "AC 2C 3C 4C 5C 6C 7C 8C 9C TC JC QC KC");
    set("f1", "AS 2S 3S 4S 5S 6S 7S 8S 9S TS JS QS KS");
    set("f2", "AH 2H 3H 4H 5H 6H 7H 8H 9H TH JH QH KH");
    set("f3", "AD 2D 3D 4D 5D 6D 7D 8D 9D TD JD QD");
    set("r0", "KD");
    CHECK(!sol_is_won() && sol_left() == 1, "one card left: not won");
    CHECK(act_double(S("r0")) && won && sol_is_won() && sol_left() == 0, "the last king home: won");
}

static void freecell(void)
{
    suite = "FreeCell";
    empty(2);
    /* SuperMoveAC_RowStack: (free cells + 1) << empty rows (one fewer to an empty row) */
    set("r0", "9D 8S 7H 6C 5D 4S");
    set("r1", "TC");
    set("r2", "KH");
    set("r3", "KS");
    set("r4", "KD");
    set("r5", "KC");
    set("r6", "QH");
    set("r7", "QS");
    CHECK(sol_can_move(S("r0"), 5), "4 free cells, no empty row: 5 cards move");
    CHECK(!sol_can_move(S("r0"), 6), "but not 6");
    set("r1", "");
    set("r6", "TC");
    CHECK(sol_can_move(S("r0"), 6) && sol_accepts(S("r6"), S("r0"), 6), "with an empty row: 6 onto TC (up to 10)");
    CHECK(sol_accepts(S("r1"), S("r0"), 5), "5 to the empty row");
    CHECK(!sol_accepts(S("r1"), S("r0"), 6), "but not 6 (the empty row is the destination)");
    set("v0", "AH");
    set("v1", "2H");
    set("v2", "3H");
    set("r6", "8C");
    set("r7", "9H");
    CHECK(sol_accepts(S("r6"), S("r0"), 4), "one free cell, one empty row: 4 onto a column");
    CHECK(!sol_accepts(S("r7"), S("r0"), 5), "but not 5");
    CHECK(!sol_accepts(S("v1"), S("r0"), 1), "a free cell holds one card (ReserveStack max_cards=1)");
    CHECK(sol_accepts(S("v3"), S("r0"), 1) && !sol_accepts(S("v3"), S("r0"), 2), "an empty cell takes one card");
    CHECK(sol_accepts(S("r1"), S("v0"), 1), "an empty row takes any card (base_rank ANY)");
    CHECK(sol_can_move(S("v0"), 1), "a free cell's card moves");
}

static void spider(void)
{
    u8 to;
    suite = "Spider";
    empty(4);           /* two suits */
    set("r0", "KC QC JC TC 9H");
    set("r1", "TH");
    CHECK(!sol_can_move(S("r0"), 2), "TC 9H: not one suit, does not move (Spider_SS_RowStack)");
    CHECK(sol_can_move(S("r0"), 1), "9H alone moves");
    CHECK(sol_accepts(S("r1"), S("r0"), 1), "9H on TH (by rank)");
    set("r1", "TC");
    CHECK(sol_accepts(S("r1"), S("r0"), 1), "9H on TC (accepts by rank: isRankSequence)");
    set("r0", "KC QC JC TC 9C");
    set("r1", "JH");
    CHECK(sol_can_move(S("r0"), 2) && sol_accepts(S("r1"), S("r0"), 2), "TC 9C onto JH");
    CHECK(sol_can_move(S("r0"), 5), "KC..9C moves whole");
    /* deal: none while a row is empty (Spider.canDealCards) */
    {
        int i;
        char nm[4];
        for (i = 0; i < 10; ++i) {
            sprintf(nm, "r%d", i);
            set(nm, "5h");
        }
        set("r9", "");
        set("t", "ac 2c 3c 4c 5c 6c 7c 8c 9c tc");
        CHECK(!sol_can_deal(), "no deal while a row is empty");
        set("r9", "5h");
        CHECK(act_click(S("t")) && !s_n[S("t")], "a deal gives each row a card");
        CHECK(!strcmp(text("r0"), "5h TC") && !strcmp(text("r9"), "5h AC"), "face up, row 0 first");
    }
    /* a full suit goes home by itself */
    empty(3);
    set("r0", "8c KC QC JC TC 9C 8C 7C 6C 5C 4C 3C 2C");
    set("r1", "AC");
    set("r2", "3c");
    CHECK(act_move(S("r1"), S("r0"), 1), "the ace completes the suit");
    CHECK(s_n[S("f0")] == 13 && !strcmp(text("r0"), "8C"), "K..A went home, the card under it turned up");
    set("r3", "KC QC JC TC 9C 8C 7C 6C 5C 4C 3C 2C AC");
    CHECK(sol_can_drop(S("r3"), &to) == 13 && to == S("f1"), "spiderCanDropCards: 13 cards to the next foundation");
    set("r3", "KC QC JC TC 9C 8C 7H 6C 5C 4C 3C 2C AC");
    CHECK(sol_can_drop(S("r3"), &to) == 0, "not when a suit is mixed in");
}

static void golf(void)
{
    suite = "Golf";
    empty(6);
    set("f0", "5H");
    set("r0", "4C");
    set("r1", "6D");
    set("r2", "7S");
    CHECK(sol_accepts(S("f0"), S("r0"), 1) && sol_accepts(S("f0"), S("r1"), 1), "4 and 6 on 5 (Golf_Waste)");
    CHECK(!sol_accepts(S("f0"), S("r2"), 1), "7 on 5 refused");
    set("f0", "KH");
    set("r0", "AC");
    set("r1", "QD");
    CHECK(!sol_accepts(S("f0"), S("r0"), 1), "an ace on a king refused (no wrap)");
    CHECK(sol_accepts(S("f0"), S("r1"), 1), "a queen on a king (strictness 0)");
    CHECK(!sol_accepts(S("r2"), S("r1"), 1), "a row takes nothing");
    CHECK(act_click(S("r1")) && !s_n[S("r1")], "a click on a row card plays it (Golf_RowStack.clickHandler)");
    set("r0", "");
    set("r2", "JC");
    set("t", "2c");
    CHECK(act_click(S("t")) && !strcmp(text("f0"), "KH QD 2C"), "the talon deals onto any card");
    CHECK(!sol_can_deal(), "one round only");
    CHECK(act_click(S("r2")) == 0 && !won, "JC on 2C refused");
    set("f0", "QD");
    CHECK(act_click(S("r2")) && won, "the rows empty: won (Golf.isGameWon)");
}

static void pyramid(void)
{
    suite = "Pyramid";
    empty(7);
    {
        int i;
        char nm[4];
        for (i = 0; i < 28; ++i) {
            sprintf(nm, "r%d", i);
            set(nm, "");
        }
    }
    /* rows: 0 the top; 1, 2 below it; ...; 21-27 the bottom row */
    set("r0", "KH");
    set("r1", "5C");
    set("r2", "8D");
    set("t", "2s");
    set("w", "");
    CHECK(sol_blocked(S("r0")), "the top card is blocked by the two under it");
    CHECK(!sol_can_move(S("r0"), 1) && act_click(S("r0")) == 0, "a blocked king does not move");
    CHECK(sol_accepts(S("r2"), S("r1"), 1), "5 + 8 = 13: a pair (Pyramid_StackMethods)");
    CHECK(act_move(S("r1"), S("r2"), 1), "the pair dropped");
    CHECK(!s_n[S("r1")] && !s_n[S("r2")] && s_n[S("f0")] == 2, "both went to the foundation");
    CHECK(act_click(S("r0")) && s_n[S("f0")] == 3, "the king, free, goes by a click");
    set("t", "9s 2s");
    set("w", "JH");
    set("r21", "AD");
    CHECK(!sol_accepts(S("w"), S("r21"), 1), "J (10) + A (0) = 10: refused");
}

static void pyramid2(void)
{
    suite = "Pyramid";
    empty(7);
    set("t", "9s 2s");
    set("w", "QH");
    set("r21", "AD");
    CHECK(act_move(S("r21"), S("w"), 1), "A on Q: a pair (0 + 11)");
    CHECK(!strcmp(text("w"), "2S") && !strcmp(text("t"), "9S"), "the waste emptied: a card dealt; the talon's top face up");
    set("r22", "JC");
    CHECK(sol_accepts(S("w"), S("r22"), 1), "J (10) + 2 (1) = 11: a pair");
    set("r22", "3C");
    CHECK(!sol_accepts(S("w"), S("r22"), 1), "3 + 2 refused");
    set("r23", "4D");
    CHECK(sol_accepts(S("t"), S("r23"), 1), "4 (3) on the talon's 9 (8): a pair on the talon (Pyramid_Talon)");
    CHECK(act_move(S("r23"), S("t"), 1) && s_n[S("t")] == 0, "the talon's card goes with it");
    CHECK(!sol_can_move(S("t"), 1), "the talon's card is not dragged (Stack.canMoveCards)");
}

static void yukon(void)
{
    suite = "Yukon";
    empty(8);
    set("r0", "3c 8D 2S 9H 4C");
    set("r1", "9S");
    set("r2", "");
    CHECK(sol_can_move(S("r0"), 4), "any face-up cards move, in sequence or not");
    CHECK(sol_accepts(S("r1"), S("r0"), 4), "8D 2S 9H 4C onto 9S: only the first card counts");
    CHECK(!sol_accepts(S("r2"), S("r0"), 4), "an empty row takes a king only");
    set("r3", "qd KC 7H");
    CHECK(sol_accepts(S("r2"), S("r3"), 2), "KC 7H to the empty row");
    CHECK(!sol_can_move(S("r3"), 3), "not the face-down queen");
    set("f0", "AC 2C");
    CHECK(!sol_can_move(S("f0"), 1), "a foundation's card stays (max_move=0)");
    suite = "Russian Solitaire";
    empty(9);
    set("r0", "8S 4H");
    set("r1", "9S");
    set("r2", "9C");
    CHECK(sol_accepts(S("r1"), S("r0"), 2) && !sol_accepts(S("r2"), S("r0"), 2), "8S on 9S, not on 9C (Yukon_SS_RowStack)");
}

/* the same deal twice is the same; the next deal is not */
static void deals(void)
{
    static char a[4096], b[4096];
    u8 g;
    suite = "deals";
    for (g = 0; g < NGAMES; ++g) {
        sol_new(g, 12345);
        sol_dump(a, 1);
        sol_new(g, 12345);
        sol_dump(b, 1);
        CHECK(!strcmp(a, b), "a deal number gives the same layout");
        sol_new(g, 12346);
        sol_dump(b, 1);
        CHECK(strcmp(a, b), "the next deal number another");
        CHECK(sol_left() == ncards - (G->layout == L_GOLF ? 17 : 0) || G->layout == L_GOLF, "cards left");
    }
    /* Microsoft's FreeCell #1, as everyone knows it (by rows) */
    sol_new(2, 1);
    CHECK(!strcmp(text("r0"), "JD KD 2S 4C 3S 6D 6S") && !strcmp(text("r7"), "5H 3H 3C 7S 7D TC"),
          "FreeCell #1 is Microsoft's deal #1");
    sol_new(2, 617);
    CHECK(!strcmp(text("r0"), "7D TD TH KD 4C 4S JD") && !strcmp(text("r7"), "AH KH TC JS 2S QH"),
          "FreeCell #617 is Microsoft's deal #617");
}

/* a move list played: won; then undone to the deal and redone to the end */
static void solve(const char *file, u8 game, u32 deal, const char *name)
{
    static char start[4096], end[4096], now[4096];
    char line[200];
    int n = 0, r, bad = 0;
    FILE *f = fopen(file, "r");
    suite = name;
    if (!f) {
        CHECK(0, "the move list");
        return;
    }
    sol_new(game, deal);
    opt_autodrop = 0;
    sol_dump(start, 0);
    while (fgets(line, sizeof line, f)) {
        r = do_line(line, 0);
        if (r == 2)
            continue;
        ++n;
        if (r != 1) {
            printf("      move %d refused: %s", n, line);
            bad = 1;
            break;
        }
        if (won && n)
            ;
    }
    fclose(f);
    CHECK(!bad, "every move of the list accepted");
    CHECK(won && sol_is_won(), "the game won");
    CHECK(moves == n, "the moves counted");
    sol_dump(end, 0);
    while (act_undo())
        ;
    sol_dump(now, 0);
    CHECK(!strcmp(now, start) && moves == 0, "undo all the way: the deal again");
    while (act_redo())
        ;
    sol_dump(now, 0);
    CHECK(!strcmp(now, end) && won, "redo all the way: won again");
}

/* every card of the deck on exactly one stack: the counts of each card
   (by its id, face up or down) as dealt */
static int census(u16 *want)
{
    u16 have[64];
    u8 s, i;
    memset(have, 0, sizeof have);
    for (s = 0; s < nstacks; ++s)
        for (i = 0; i < s_n[s]; ++i)
            ++have[CARDID(s_c[s][i])];
    return !memcmp(have, want, sizeof have);
}

/* random play: each game, deals 1-40, 300 actions each (a legal drag of
   any run onto any stack, a click on any stack, a double click, undo,
   redo); after every action the deck whole; undo all the way gives the
   deal back. The Klondike talon is dealt to its end and turned over
   many times on the way */
static void random_play(void)
{
    static char start[4000], now[4000];
    u16 want[64];
    u8 g, s, t, n, i;
    u32 d;
    int k, lost = 0, rounds = 0, turned = 0, acts = 0, back = 1;
    suite = "random play";
    srand(1);
    for (g = 0; g < NGAMES; ++g)
        for (d = 1; d <= 40; ++d) {
            sol_new(g, d);
            opt_autodrop = (u8)(d & 1);
            memset(want, 0, sizeof want);
            for (s = 0; s < nstacks; ++s)
                for (i = 0; i < s_n[s]; ++i)
                    ++want[CARDID(s_c[s][i])];
            sol_dump(start, 0);
            for (k = 0; k < 300 && !won; ++k) {
                int r = rand() % 16;
                u8 before = round_no;
                if (r < 9) {
                    /* a legal drag: a random source and run, the first
                       stack that takes it */
                    s = (u8)(rand() % nstacks);
                    if (!s_n[s])
                        continue;
                    n = (u8)(1 + rand() % s_n[s]);
                    if (!sol_can_move(s, n))
                        n = 1;
                    if (!sol_can_move(s, n))
                        continue;
                    for (t = 0; t < nstacks; ++t)
                        if (t != s && sol_stack_is_open(t) && sol_accepts(t, s, n))
                            break;
                    if (t < nstacks)
                        acts += act_move(s, t, n);
                } else if (r < 12) {
                    acts += act_click(st_talon == ANY || rand() % 3 ? (u8)(rand() % nstacks) : st_talon);
                    if (G->talon != T_INITIAL)
                        acts += act_click(st_talon);
                } else if (r < 14) {
                    acts += act_double((u8)(rand() % nstacks));
                } else if (r == 14) {
                    act_undo();
                } else {
                    act_redo();
                }
                if (round_no != before)
                    ++turned;
                if (!census(want)) {
                    printf("      game %d deal %lu action %d: a card lost or doubled\n", g, (unsigned long)d, k);
                    lost = 1;
                    break;
                }
            }
            rounds += round_no > 1;
            while (act_undo())
                if (!census(want))
                    lost = 1;
            sol_dump(now, 0);
            if (strcmp(now, start))
                back = 0;
        }
    CHECK(!lost, "after every action of random play, every card on one stack");
    CHECK(back, "undo all the way gives each deal back");
    CHECK(turned > 100 && rounds > 50, "the talons dealt out and turned over (waste recycled)");
    printf("      random play: %d actions done, %d talons turned over, in %d games\n", acts, turned, rounds);
}

/* sol_has_move: the status bar's "No moves left" */
static void stuck(void)
{
    suite = "no moves left";
    /* the photo's kind of table: talon and waste empty, every card face
       up in the rows, nothing home */
    empty(0);
    set("r0", "KH QS JH TS 9H 8S 7H 6S 5H 4S 3H 2S");
    set("r1", "KS QH JS TH 9S 8H 7S 6H 5S 4H 3S 2H");
    set("r2", "KC QD JC TD 9C 8D 7C 6D 5C 4D 3C 2D");
    set("r3", "KD QC JD TC 9D 8C 7D 6C 5D 4C 3D 2C");
    set("r4", "");
    CHECK(!sol_has_move(), "the rows' runs whole, the talon and waste empty, no ace: no move left");
    set("r3", "KD QC JD TC 9D 8C 7D 6C 5D 4C 3D 2C AH");
    CHECK(sol_has_move(), "an ace on top: it goes home");
    set("r3", "KD QC JD TC 9D 8C 7D 6C 5D 4C 3D 2C");
    set("r4", "AH");
    CHECK(sol_has_move(), "the ace alone in a row: home too");
    set("r4", "");
    set("r2", "KC QD JC TD 9C 8D 7C 6D 5C 4D 3C");
    set("r5", "2D");
    CHECK(sol_has_move(), "2D alone: onto 3C, or 3C onto it...");
    set("r2", "KC QD JC TD 9C 8D 7C 6D 5C 4D 3C 2D");
    set("r5", "");
    set("r0", "8D KH QS JH TS 9H 8S 7H 6S 5H 4S 3H 2S");
    CHECK(sol_has_move(), "the king's run off 8D onto the empty row frees it");
    /* the user's photo: 8D on top of a row, 9C on top of another */
    set("r0", "KH QS JH TS 9H 8S 7H 6S 5H 4S 3H 2S 9C");
    set("r1", "KS QH JS TH 9S 8H 7S 6H 5S 4H 3S 2H 8D");
    CHECK(sol_has_move(), "8D onto 9C");
    /* the talon turned over without end: a deal is a move only if a card
       of the talon or the waste would go somewhere */
    empty(0);
    set("r0", "KH QS JH TS 9H 8S 7H 6S 5H 4S 3H 2S");
    set("t", "jd tc");
    set("w", "5C");
    CHECK(!sol_has_move(), "Klondike: talon and waste cards that go nowhere: no move");
    set("t", "jd ac");
    CHECK(sol_has_move(), "an ace in the talon: dealing gets to it");
    set("t", "jd tc");
    set("w", "AD 5C");
    CHECK(sol_has_move(), "an ace under the waste's top: it comes round");
    set("w", "");
    set("t", "");
    set("f0", "AC 2C");
    set("r0", "KH QS JH TS 9H 8S 7H 6S 5H 4S 3H");
    CHECK(sol_has_move(), "a foundation's card back onto a row (max_move 1)");
    /* Golf: the talon dealt once only */
    empty(6);
    set("r0", "KS");
    set("f0", "9C");
    set("t", "5c");
    CHECK(sol_has_move(), "Golf: a deal left");
    set("t", "");
    CHECK(!sol_has_move(), "Golf: the talon out, KS not on 9C: no move");
    /* every deal of every game starts with a move */
    {
        u8 g;
        u32 d;
        int all = 1;
        for (g = 0; g < NGAMES; ++g)
            for (d = 1; d <= 20; ++d) {
                sol_new(g, d);
                if (!sol_has_move())
                    all = 0;
            }
        CHECK(all, "every game's deals 1-20 have a move at the start");
    }
}

int main(int argc, char **argv)
{
    klondike();
    freecell();
    spider();
    golf();
    pyramid();
    pyramid2();
    yukon();
    deals();
    random_play();
    stuck();
    solve(argc > 1 ? argv[1] : "test/freecell1.moves", 2, 1, "FreeCell #1 solution");
    solve(argc > 2 ? argv[2] : "test/klondike1.moves", 0, 1, "Klondike #1 solution");
    printf("%s  %d checks, %d failed\n", fails ? "FAIL" : "ok   ", checks, fails);
    return fails != 0;
}
