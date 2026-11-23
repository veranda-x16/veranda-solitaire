/*
 * Veranda Solitaire: the engine's interface (portable C: cc65 for the
 * X16, gcc for the host tests).
 *
 * The model is PySolFC's (pysollib/stack.py, pysollib/game/__init__.py):
 * a game is a set of stacks (talon, waste, foundations, rows, reserves),
 * each of a class with capabilities (suit, base rank, direction, how many
 * cards it may move and accept); acceptsCards, canMoveCards, canDropCards
 * and canFlipCard are its rules; a deal program puts the shuffled cards
 * out; autoPlay flips, drops and deals after every move. Games are rows
 * of a table (games.c), as PySolFC's are a few lines of class definitions.
 *
 * Copyright (C) 2026 the Veranda Solitaire authors; the rules are
 * translated from PySolFC (see NOTICE). GPL-3.0-or-later (see COPYING).
 */
#ifndef SOL_H
#define SOL_H

typedef unsigned char u8;
typedef signed char s8;
typedef unsigned int u16;
#ifdef __CC65__
typedef unsigned long u32;
#else
#include <stdint.h>
typedef uint32_t u32;
#endif

/*---------------------------------------------------------------
  cards: bit 7 face up, bits 0-5 suit * 13 + rank
  suits as PySolFC's: 0 clubs, 1 spades, 2 hearts, 3 diamonds;
  colour = suit / 2 (0 black, 1 red); ranks 0 ace .. 12 king
 ---------------------------------------------------------------*/
#define FACEUP   0x80
#define CARDID(c) ((c) & 0x3F)
#define ACE      0
#define KING     12
#define ANY      0xFF
extern const u8 rank_of[52], suit_of[52];
#define RANK(c)  (rank_of[CARDID(c)])
#define SUIT(c)  (suit_of[CARDID(c)])
#define COLOR(c) (SUIT(c) >> 1)
#define UP(c)    ((c) & FACEUP)

/*---------------------------------------------------------------
  stack classes (PySolFC's names in the comments)
 ---------------------------------------------------------------*/
enum {
    K_TALON,        /* the talon: its kind is the game's talon field */
    K_WASTE,        /* WasteStack */
    K_FOUND,        /* SS_FoundationStack */
    K_SPIDERF,      /* Spider_SS_Foundation */
    K_ROW,          /* SequenceRowStack: AC_RowStack, SS_RowStack ... */
    K_YUKON,        /* Yukon_AC_RowStack, Yukon_SS_RowStack */
    K_RESERVE,      /* ReserveStack (a free cell) */
    K_GOLFROW,      /* Golf_RowStack */
    K_GOLFWASTE,    /* Golf_Waste (also the foundation) */
    K_PYRROW,       /* Pyramid_RowStack */
    K_PYRWASTE,     /* Pyramid_Waste */
    K_PYRFOUND      /* Pyramid_Foundation */
};

/* talon kinds */
enum {
    T_INITIAL,      /* InitialDealTalonStack: only for the deal */
    T_WASTE,        /* WasteTalonStack (num_deal, max_rounds) */
    T_FACEUP,       /* Pyramid_Talon: FaceUpWasteTalonStack, accepts pairs */
    T_DEALROW       /* DealRowTalonStack (Spider: no empty row to deal) */
};

/* sequences (stack.py's is...Sequence) */
enum { SEQ_AC, SEQ_SS, SEQ_RK, SEQ_SC, SEQ_BO };

/* row flags */
#define RF_SUPER   1    /* SuperMove..._RowStack: moves limited by free cells, doubled by empty rows */
#define RF_SPIDER  2    /* Spider_..._RowStack: accepts by rank, moves by the sequence */
#define RF_FREE    4    /* FreeCell_..._RowStack: moves limited by free cells + 1 */

/* game flags */
#define GF_WIN_ROWS    1    /* won when the rows are empty (Golf) */
#define GF_NOAUTO      2    /* no auto flip or drop (Golf) */
#define GF_RUNS        4    /* a full suit run drops by itself (Spider) */
#define GF_WONDEAL     8    /* the talon does not deal once won (Golf, Pyramid) */
#define GF_SPIDERDEAL  16   /* no deal while a row is empty (Spider) */
#define GF_PAIRS       32   /* pairs of 13 (Pyramid) */
#define GF_DRAW3       64   /* the Klondike draw 3 twin of the game before it */

/* deal programs (PySolFC's startGame as data) */
enum {
    D_END,
    D_ROWS,         /* a, b, flags: rows a..b, one card each */
    D_LIST,         /* n, flags, i1..in */
    D_CARDS         /* the talon's dealCards (the first card to the waste) */
};
#define DF_FLIP 1
#define DF_REV  2

/* layouts (layout.c) */
enum { L_KLONDIKE, L_FREECELL, L_SPIDER, L_GOLF, L_PYRAMID, L_YUKON };

struct gdef {
    const char *name;
    u8 decks;
    const u8 *suits;        /* 4 suit codes a deck (PySolFC's gameinfo.suits) */
    u8 talon;               /* T_... */
    u8 num_deal;            /* cards a deal (WasteTalonStack) */
    u8 max_rounds;          /* 0: unlimited (PySolFC's -1) */
    u8 waste;               /* 0 none, else the waste's class */
    u8 nfound, fkind, fmaxmove;
    u8 nrows, rkind, rseq, rflags, rbase;
    u8 nres;
    const u8 *deal;
    u8 flags;
    u8 layout;
    u16 pysol_id;           /* PySolFC's game id (gamedb), for the record */
};

#define NGAMES 10
extern const struct gdef games[NGAMES];

/*---------------------------------------------------------------
  the state
 ---------------------------------------------------------------*/
#define MAXSTACKS 32
#define POOL      2100

extern u8 gid;                      /* the game */
extern const struct gdef *G;
extern u8 nstacks;
extern u8 s_kind[MAXSTACKS], s_n[MAXSTACKS], s_suit[MAXSTACKS], s_brank[MAXSTACKS];
extern s8 s_dir[MAXSTACKS];
extern u8 s_maxmove[MAXSTACKS], s_maxacc[MAXSTACKS], s_maxcards[MAXSTACKS], s_minacc[MAXSTACKS];
extern u8 s_seq[MAXSTACKS], s_rf[MAXSTACKS], s_cap[MAXSTACKS];
extern u8 s_block[MAXSTACKS][2];    /* Pyramid: the two cards below it */
extern u8 *s_c[MAXSTACKS];          /* the cards, bottom first */
extern u8 st_talon, st_waste, st_f0, st_r0, st_v0;  /* first of each group (st_waste ANY: none) */
extern u8 n_found, n_rows, n_res;
extern u8 round_no;                 /* the talon's round, from 1 */
extern u32 deal_no;
extern u16 moves;                   /* the player's moves (undo counts back) */
extern u8 won;
extern u8 opt_autodrop;
extern u8 ncards;

#define TOP(s) (s_c[s][s_n[s] - 1])

/* the engine */
void sol_new(u8 game, u32 deal);            /* set up, shuffle, deal */
u8 sol_accepts(u8 to, u8 from, u8 n);       /* the top n cards of from onto to */
u8 sol_can_move(u8 s, u8 n);                /* the top n of s may be picked up */
u8 sol_can_flip(u8 s);
u8 sol_can_drop(u8 s, u8 *to);              /* to a foundation: the count, 0 none */
u8 sol_can_deal(void);
u8 sol_has_move(void);                      /* a move left (deals that come round: none) */
u8 sol_is_won(void);
u8 sol_blocked(u8 s);

/* the player's actions: each is one undo step; they return 0 if nothing was done */
u8 act_move(u8 from, u8 to, u8 n);          /* a drag dropped (accepts checked) */
u8 act_click(u8 s);                         /* a click: deal, Golf's drop, Pyramid's king, flip */
u8 act_double(u8 s);                        /* a double click: flip or to a foundation */
u8 act_undo(void);
u8 act_redo(void);
u8 sol_left(void);                          /* cards not on the foundations */
u8 sol_stack_is_drop(u8 s);                 /* in sg.dropstacks */
u8 sol_stack_is_open(u8 s);                 /* in sg.openstacks (drop targets) */

/* the move log, as atoms (the platform keeps them: host.c, the X16's banks) */
#define A_MOVE   1      /* a: from, b: to, n */
#define A_FLIP   2      /* a: the stack */
#define A_TURN   3      /* a: from, b: to (all its cards, reversed, face down) */
#define A_ROUND  4      /* the talon's round + 1 */
#define A_END    0x80   /* the last atom of a move */
u8 log_put(u8 op, u8 a, u8 b, u8 n);        /* 0 if full */
void log_get(u16 i, u8 *atom);              /* atom i (4 bytes) */
void log_mark_end(u16 i);                   /* set A_END on atom i */
extern u16 log_top, log_cur;                /* atoms recorded, atoms done */

/* the platform hears of each stack change (to redraw): s, the lowest card index changed */
void sol_changed(u8 s, u8 from);

/* the random numbers: PySolFC's LCRandom31 (pysol_cards.random) */
void lc_seed(u32 seed);
u16 lc_rand(void);

#endif
