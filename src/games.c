/*
 * Veranda Solitaire: the games, as data. Each row is PySolFC's class for
 * the game (pysollib/games/, a .py each) written as the engine's fields; the deal
 * programs are the classes' startGame.
 *
 * Copyright (C) 2026 the Veranda Solitaire authors; derived from PySolFC
 * (see NOTICE). GPL-3.0-or-later (see COPYING).
 */
#include "sol.h"

/* gameinfo.suits: a deck's four suits */
static const u8 suits4[4] = { 0, 1, 2, 3 };
static const u8 suits1[4] = { 0, 0, 0, 0 };     /* Spider (1 Suit): suits=(0, 0, 0, 0) */
static const u8 suits2[4] = { 0, 0, 2, 2 };     /* Spider (2 Suits): suits=(0, 0, 2, 2) */

/* Klondike.startGame(flip=0, reverse=1): rows[i:] face down for i in
   1..6, right to left; then every row face up; then dealCards */
static const u8 deal_klondike[] = {
    D_ROWS, 1, 6, DF_REV, D_ROWS, 2, 6, DF_REV, D_ROWS, 3, 6, DF_REV,
    D_ROWS, 4, 6, DF_REV, D_ROWS, 5, 6, DF_REV, D_ROWS, 6, 6, DF_REV,
    D_ROWS, 0, 6, DF_REV | DF_FLIP, D_CARDS, D_END };

/* FreeCell.startGame: _startDealNumRows(5), dealRow(), dealRow(rows=r[:4]) */
static const u8 deal_freecell[] = {
    D_ROWS, 0, 7, DF_FLIP, D_ROWS, 0, 7, DF_FLIP, D_ROWS, 0, 7, DF_FLIP,
    D_ROWS, 0, 7, DF_FLIP, D_ROWS, 0, 7, DF_FLIP, D_ROWS, 0, 7, DF_FLIP,
    D_ROWS, 0, 3, DF_FLIP, D_END };

/* RelaxedSpider.startGame(flip=0): 4 rows face down, rows 0, 3, 6, 9
   face down, then _startAndDealRow (face up) */
static const u8 deal_spider[] = {
    D_ROWS, 0, 9, 0, D_ROWS, 0, 9, 0, D_ROWS, 0, 9, 0, D_ROWS, 0, 9, 0,
    D_LIST, 4, 0, 0, 3, 6, 9, D_ROWS, 0, 9, DF_FLIP, D_END };

/* Golf.startGame(num_rows=5): _startDealNumRows(4), dealRow(), dealCards() */
static const u8 deal_golf[] = {
    D_ROWS, 0, 6, DF_FLIP, D_ROWS, 0, 6, DF_FLIP, D_ROWS, 0, 6, DF_FLIP,
    D_ROWS, 0, 6, DF_FLIP, D_ROWS, 0, 6, DF_FLIP, D_CARDS, D_END };

/* Pyramid.startGame: dealRow(), dealCards() */
static const u8 deal_pyramid[] = { D_ROWS, 0, 27, DF_FLIP, D_CARDS, D_END };

/* Yukon.startGame: rows[i:] face down for i in 1..6; 4 x rows[1:] face
   up; _startAndDealRow */
static const u8 deal_yukon[] = {
    D_ROWS, 1, 6, 0, D_ROWS, 2, 6, 0, D_ROWS, 3, 6, 0,
    D_ROWS, 4, 6, 0, D_ROWS, 5, 6, 0, D_ROWS, 6, 6, 0,
    D_ROWS, 1, 6, DF_FLIP, D_ROWS, 1, 6, DF_FLIP, D_ROWS, 1, 6, DF_FLIP,
    D_ROWS, 1, 6, DF_FLIP, D_ROWS, 0, 6, DF_FLIP, D_END };

const struct gdef games[NGAMES] = {
    /* Klondike (2): WasteTalonStack(max_rounds=-1, num_deal=1), WasteStack,
       SS_FoundationStack, KingAC_RowStack x 7 */
    { "Klondike", 1, suits4, T_WASTE, 1, 0, K_WASTE, 4, K_FOUND, 1,
      7, K_ROW, SEQ_AC, 0, KING, 0, deal_klondike, 0, L_KLONDIKE, 2 },
    /* Klondike (Draw 3) (18): KlondikeByThrees, num_deal=3 */
    { "Klondike (Draw 3)", 1, suits4, T_WASTE, 3, 0, K_WASTE, 4, K_FOUND, 1,
      7, K_ROW, SEQ_AC, 0, KING, 0, deal_klondike, GF_DRAW3, L_KLONDIKE, 18 },
    /* FreeCell (8): InitialDealTalonStack, SS_FoundationStack,
       SuperMoveAC_RowStack x 8, ReserveStack x 4 */
    { "FreeCell", 1, suits4, T_INITIAL, 1, 1, 0, 4, K_FOUND, 1,
      8, K_ROW, SEQ_AC, RF_SUPER, ANY, 4, deal_freecell, 0, L_FREECELL, 8 },
    /* Spider (1 Suit) (269): DealRowTalonStack, Spider_SS_Foundation x 8,
       Spider_RowStack x 10 (Spider: no deal while a row is empty) */
    { "Spider (1 Suit)", 2, suits1, T_DEALROW, 1, 1, 0, 8, K_SPIDERF, 0,
      10, K_ROW, SEQ_SS, RF_SPIDER, ANY, 0, deal_spider, GF_RUNS | GF_SPIDERDEAL, L_SPIDER, 269 },
    /* Spider (2 Suits) (270) */
    { "Spider (2 Suits)", 2, suits2, T_DEALROW, 1, 1, 0, 8, K_SPIDERF, 0,
      10, K_ROW, SEQ_SS, RF_SPIDER, ANY, 0, deal_spider, GF_RUNS | GF_SPIDERDEAL, L_SPIDER, 270 },
    /* Spider (11): four suits */
    { "Spider (4 Suits)", 2, suits4, T_DEALROW, 1, 1, 0, 8, K_SPIDERF, 0,
      10, K_ROW, SEQ_SS, RF_SPIDER, ANY, 0, deal_spider, GF_RUNS | GF_SPIDERDEAL, L_SPIDER, 11 },
    /* Golf (36): Golf_Talon(max_rounds=1), Golf_Waste (the foundation),
       Golf_RowStack x 7; won with the rows empty; no auto flip or drop */
    { "Golf", 1, suits4, T_WASTE, 1, 1, K_GOLFWASTE, 1, K_GOLFWASTE, 0,
      7, K_GOLFROW, 0, 0, ANY, 0, deal_golf, GF_WIN_ROWS | GF_NOAUTO | GF_WONDEAL, L_GOLF, 36 },
    /* Pyramid (38): Pyramid_Talon(max_rounds=3, max_accept=1), Pyramid_Waste,
       Pyramid_Foundation, Pyramid_RowStack x 28 */
    { "Pyramid", 1, suits4, T_FACEUP, 1, 3, K_PYRWASTE, 1, K_PYRFOUND, 0,
      28, K_PYRROW, 0, 0, ANY, 0, deal_pyramid, GF_PAIRS | GF_WONDEAL, L_PYRAMID, 38 },
    /* Yukon (19): InitialDealTalonStack, SS_FoundationStack(max_move=0),
       Yukon_AC_RowStack(base_rank=KING) x 7 */
    { "Yukon", 1, suits4, T_INITIAL, 1, 1, 0, 4, K_FOUND, 0,
      7, K_YUKON, SEQ_AC, 0, KING, 0, deal_yukon, 0, L_YUKON, 19 },
    /* Russian Solitaire (20): Yukon with Yukon_SS_RowStack */
    { "Russian Solitaire", 1, suits4, T_INITIAL, 1, 1, 0, 4, K_FOUND, 0,
      7, K_YUKON, SEQ_SS, 0, KING, 0, deal_yukon, 0, L_YUKON, 20 },
};
