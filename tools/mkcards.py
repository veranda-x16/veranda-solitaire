#!/usr/bin/env python3
"""Veranda Solitaire's cards, drawn here: 56 x 80 pixels, in colour
(Veranda Look 3 "Sunroom") and in black and white.

    mkcards.py <outdir>

A card is two layers, as Veranda's tile mode shows a screen (Veranda's
DECISIONS V101, V105; the SDK's vrpic.h): the ink (the 1 bpp bitmap: the
outline, the black suits, the figures' lines) and under it 8 x 8 cards
of 4 bpp tiles (the colour: the face, the red suits, the figures' robes
and crowns, the back). A card's picture starts on an 8 x 8 card of the
screen, so its 7 x 10 cards are tiles of its own; the rotated half of a
card is the same tiles flipped.

Writes into <outdir>:
  cards.1 ...  the overlay records: PER_RECORD pictures of PIC_REC bytes:
               the ink (7 columns of 80 bytes, bit 7 the left pixel, 1 =
               ink) and the map (70 entries of 2 bytes, row by row: VERA's
               map entry, the tile's low byte, then palette row << 4 |
               vflip << 3 | hflip << 2 | the tile's bits 8-9)
  tiles.1 ...  the tiles for vr_vram_load (3 bytes the VRAM address, 2
               the length, the bytes; a length 0 ends)
  tiles.bin    the tiles alone (the host's drawing)
  cards.inc    the felt's and the back's tiles (blit.s)
  cards.h      the picture numbers, the tiles, the palette rows (light
               and dark), the corner masks
  cards.png    every picture, light and dark, for review (with Pillow)

Pictures 0-51: the faces in colour, suit * 13 + rank (suits as PySolFC's:
clubs, spades, hearts, diamonds; the red suits' marks are colour, not
ink); 52 the back; 53 an empty place; 54-57 the foundations' places (a
suit each); 58 Spider's foundation (K); 59 the talon's place when it can
turn the waste over again; 60-85 the hearts and diamonds in black and
white (their marks in ink, the pips outlined: the Classic theme, the
win's flying cards). The black suits' faces and the others are the same
ink in both looks.

The corner cards of a picture come in threes of consecutive tiles: the
rounded corner's outside the felt, a face, the back (what lies under it:
draw.c reads the screen's map entry there).

Nothing here comes from another card set or product: the pips are simple
geometry, the ranks a pixel font made for this, the court figures and the
back drawn here. GPL-3.0-or-later (see COPYING).
"""
import math
import os
import sys

W, H, R = 56, 80, 4
PER_RECORD = 11
INK_BYTES = W * H // 8           # 560
MAP_BYTES = (W // 8) * (H // 8) * 2    # 140
PIC_REC = INK_BYTES + MAP_BYTES  # 700
TILE0 = 16                       # the first tile (0-15 are vr_pic's solid ones)
TILE_ADDR = 0x13000              # VR_PIC_TILES
TILES_PER_RECORD = 240
ROW_RED, ROW_BLACK = 2, 3        # the palette rows

# palette row 2's entries (row 3: RED and BLUE change places: the black
# suits' court figures in blue)
FACE, RED, GOLD, SKIN, BLUE, FELT, BACK, SLOT, HAIR = range(1, 10)
LIGHT = {FACE: 0xFFF, RED: 0xD22, GOLD: 0xEB3, SKIN: 0xFDB, BLUE: 0x36B, FELT: 0x7A8, BACK: 0x47A,
         SLOT: 0x697, HAIR: 0x953}
DARK = {FACE: 0x445, RED: 0xF66, GOLD: 0xCA4, SKIN: 0xDA8, BLUE: 0x69D, FELT: 0x343, BACK: 0x247,
        SLOT: 0x232, HAIR: 0x864}
INK_LIGHT, INK_DARK, PAPER_LIGHT, PAPER_DARK = 0x223, 0xEEF, 0xFFF, 0x334


def palette_row(tab, row):
    out = [0] * 16
    for k, v in tab.items():
        out[k] = v
    if row == ROW_BLACK:
        out[RED], out[BLUE] = out[BLUE], out[RED]
    return out


# --------------------------------------------------------------- shapes


def card_inside(x, y):
    """the card's rounded rectangle (pixel centres)"""
    px, py = x + 0.5, y + 0.5
    for cx, cy in ((R, R), (W - R, R), (R, H - R), (W - R, H - R)):
        if (px < R) == (cx == R) and (py < R) == (cy == R) and \
                (px < R or px > W - R) and (py < R or py > H - R):
            return (px - cx) ** 2 + (py - cy) ** 2 <= R * R
    return True


def heart(u, v):
    if (u + 0.48) ** 2 + (v + 0.38) ** 2 <= 0.52 ** 2 or (u - 0.48) ** 2 + (v + 0.38) ** 2 <= 0.52 ** 2:
        return True
    return -0.3 <= v <= 0.95 and abs(u) <= 0.98 * (0.95 - v) / 1.25


def diamond(u, v):
    return abs(u) / 0.74 + abs(v) / 0.98 <= 1


def stem(u, v):
    if abs(u) <= 0.11 and 0.2 <= v <= 0.95:
        return True
    return 0.62 <= v <= 0.97 and abs(u) <= (v - 0.62) * 1.35 + 0.05


def spade(u, v):
    if stem(u, v):
        return True
    w, z = u, -(v - 0.12) * 1.0        # a heart upside down, a little up
    if (w + 0.46) ** 2 + (z + 0.3) ** 2 <= 0.47 ** 2 or (w - 0.46) ** 2 + (z + 0.3) ** 2 <= 0.47 ** 2:
        return v <= 0.62
    return -0.25 <= z <= 1.05 and abs(w) <= 0.93 * (1.05 - z) / 1.32


def club(u, v):
    if stem(u, v):
        return True
    r = 0.36
    for cx, cy in ((0, -0.5), (-0.48, 0.08), (0.48, 0.08)):
        if (u - cx) ** 2 + (v - cy) ** 2 <= r * r:
            return True
    return u * u + (v + 0.05) ** 2 <= 0.22 ** 2


SHAPES = [club, spade, heart, diamond]


def raster(shape, size, solid=True, thick=1):
    """a size x size picture of a suit: solid, or its outline"""
    ss = 5
    on = [[False] * size for _ in range(size)]
    for j in range(size):
        for i in range(size):
            n = 0
            for b in range(ss):
                for a in range(ss):
                    u = ((i + (a + 0.5) / ss) / size) * 2 - 1
                    v = ((j + (b + 0.5) / ss) / size) * 2 - 1
                    n += shape(u, v)
            on[j][i] = n * 2 >= ss * ss
    if solid:
        return on
    inner = on
    for _ in range(thick):
        inner = [[inner[j][i] and 0 < i < size - 1 and 0 < j < size - 1 and inner[j - 1][i]
                  and inner[j + 1][i] and inner[j][i - 1] and inner[j][i + 1]
                  for i in range(size)] for j in range(size)]
    return [[on[j][i] and not inner[j][i] for i in range(size)] for j in range(size)]


def bits(rows):
    return [[ch == '#' for ch in r] for r in rows]


# the index: a bold rank 6 x 7 and a pip 7 x 7, side by side in rows 2-8
# (a face-up card under another shows 16 rows, 8 when squeezed)
GLYPHS = {
    'A': ["..##..", ".####.", "##..##", "##..##", "######", "##..##", "##..##"],
    '2': [".####.", "##..##", "....##", "..###.", ".##...", "##....", "######"],
    '3': [".####.", "##..##", "....##", "..###.", "....##", "##..##", ".####."],
    '4': ["...##.", "..###.", ".####.", "##.##.", "######", "...##.", "...##."],
    '5': ["######", "##....", "#####.", "....##", "....##", "##..##", ".####."],
    '6': [".####.", "##....", "#####.", "##..##", "##..##", "##..##", ".####."],
    '7': ["######", "....##", "...##.", "..##..", "..##..", ".##...", ".##..."],
    '8': [".####.", "##..##", "##..##", ".####.", "##..##", "##..##", ".####."],
    '9': [".####.", "##..##", "##..##", ".#####", "....##", "...##.", ".###.."],
    'T': [".#.##.", "##.#.#", ".#.#.#", ".#.#.#", ".#.#.#", ".#.#.#", ".#.##."],
    'J': ["..####", "....##", "....##", "....##", "##..##", "##..##", ".####."],
    'Q': [".####.", "##..##", "##..##", "##..##", "##.###", ".####.", "....##"],
    'K': ["##..##", "##.##.", "####..", "###...", "####..", "##.##.", "##..##"],
}
RANKS = 'A23456789TJQK'
SMALL = [bits(p) for p in (
    ["..###..", "..###..", "##.#.##", "#######", "##.#.##", "...#...", "..###.."],
    ["...#...", "..###..", ".#####.", "#######", ".##.##.", "...#...", "..###.."],
    [".##.##.", "#######", "#######", "#######", ".#####.", "..###..", "...#..."],
    ["...#...", "..###..", ".#####.", "#######", ".#####.", "..###..", "...#..."],
)]
SMALL_OUT = SMALL[:2] + [bits(p) for p in (            # the black-and-white red suits
    [".##.##.", "#..#..#", "#.....#", "#.....#", ".#...#.", "..#.#..", "...#..."],
    ["...#...", "..#.#..", ".#...#.", "#.....#", ".#...#.", "..#.#..", "...#..."],
)]
PIP = [bits(p) for p in (
    ["...###...", "..#####..", "..#####..", ".##.#.##.", "#########", "#########", ".##.#.##.",
     "....#....", "..#####.."],
    ["....#....", "...###...", "..#####..", ".#######.", "#########", "#########", ".##.#.##.",
     "....#....", "...###..."],
    [".##...##.", "####.####", "#########", "#########", "#########", ".#######.", "..#####..",
     "...###...", "....#...."],
    ["....#....", "...###...", "..#####..", ".#######.", "#########", ".#######.", "..#####..",
     "...###...", "....#...."],
)]
PIP_OUT = PIP[:2] + [bits(p) for p in (
    [".##...##.", "#..#.#..#", "#...#...#", "#.......#", "#.......#", ".#.....#.", "..#...#..",
     "...#.#...", "....#...."],
    ["....#....", "...#.#...", "..#...#..", ".#.....#.", "#.......#", ".#.....#.", "..#...#..",
     "...#.#...", "....#...."])]
BIG = [raster(s, 27) for s in SHAPES]
BIG_OUT = BIG[:2] + [raster(heart, 27, False, 2), raster(diamond, 27, False, 2)]
MIDO = [raster(s, 21, False) for s in SHAPES]

# pip places (column centre x, fraction down) of the number cards
LX = {'L': 15, 'C': 28, 'R': 41}
PIPS = {
    2: [('C', 0), ('C', 1)],
    3: [('C', 0), ('C', .5), ('C', 1)],
    4: [('L', 0), ('R', 0), ('L', 1), ('R', 1)],
    5: [('L', 0), ('R', 0), ('C', .5), ('L', 1), ('R', 1)],
    6: [('L', 0), ('R', 0), ('L', .5), ('R', .5), ('L', 1), ('R', 1)],
    7: [('L', 0), ('R', 0), ('C', .25), ('L', .5), ('R', .5), ('L', 1), ('R', 1)],
    8: [('L', 0), ('R', 0), ('C', .25), ('L', .5), ('R', .5), ('C', .75), ('L', 1), ('R', 1)],
    9: [(c, f) for c in 'LR' for f in (0, 1 / 3, 2 / 3, 1)] + [('C', .5)],
    10: [(c, f) for c in 'LR' for f in (0, 1 / 3, 2 / 3, 1)] + [('C', 1 / 6), ('C', 5 / 6)],
}
Y0, Y1 = 21, 58

# --------------------------------------------------------------- the courts
# The top half of a court figure inside its frame (34 x 27 pixels, the
# card's x 11-44, y 13-39); the bottom half is it turned round. K ink,
# r the suit's colour (row 2 red, row 3 blue), b the other, g gold, s
# skin, h hair, . the face.
KING = [
    "..................................",
    "...........K....KK....K...........",
    "..........KgK..KggK..KgK..........",
    "..........KggKKggggKKggK..........",
    "..........KggggggggggggK..........",
    "..........KgbgggbbgggbgK..........",
    "..........KKKKKKKKKKKKKK..........",
    "..........KhsssssssssshK..........",
    "..........KhsKKssssKKshK..........",
    "..........KhssKssssKsshK..........",
    "..........KhssssKKsssshK..........",
    "..........KhshhhhhhhhshK..........",
    "..........KhhhhKKKKhhhhK..........",
    "...........KhhhhhhhhhhK...........",
    "....KKKKKKKKKhhhhhhhhKKKKKKKKK....",
    "...KrrrrrrrgKhhhhhhhhKgrrrrrrrK...",
    "..KrrrrrrrrggKhhhhhhKggrrrrrrrrK..",
    ".KrrrrrrrrrrggKhhhhKggrrrrrrrrrrK.",
    ".KrrrrrrrrrrrggKhhKggrrrrrrrrrrrK.",
    ".KrrrrrrrrrrrrggKKggrrrrrrrrrrrrK.",
    ".KbbbbKrrrrrrrrggggrrrrrrrrKbbbbK.",
    ".KbbbbKrrrrrrrrgKggrrrrrrrrKbbbbK.",
    ".KbbbbKrrrrrrrrggggrrrrrrrrKbbbbK.",
    ".KKKKKKrrrrrrrrggggrrrrrrrrKKKKKK.",
    ".KsssKrrrrrrrrrgKggrrrrrrrrrKsssK.",
    ".KKKKKrrrrrrrrrggggrrrrrrrrrKKKKK.",
    ".KrrrrrrrrrrrrrggggrrrrrrrrrrrrrK.",
]
QUEEN = [
    "...........gg...gg...gg...........",
    "..........KggK.KggK.KggK..........",
    "..........KggKKKggKKKggK..........",
    "..........KggggggggggggK..........",
    "..........KgbgggbbgggbgK..........",
    "........KKKKKKKKKKKKKKKKKK........",
    "........KhhhssssssssssshhK........",
    "........KhhhssKssssKsshhhK........",
    "........KhhhssssssssssshhK........",
    "........KhhhssssKKsssshhhK........",
    "........KhhhhssssssssshhhK........",
    "........KhhhhhhKssssKhhhhK........",
    "........KhhhhhggggggghhhhK........",
    "...KKKKKKhhhbbbbbbbbbbhhhKKKKKK...",
    "..KrrrrrrhhhbbbbbbbbbbhhhrrrrrrK..",
    ".KrrrrrrrrhhgbbbbbbbbghhrrrrrrrrK.",
    ".KrrrrrrrrrhgbbbbbbbbghrrrrrrrrrK.",
    ".KrrrrrrrrrrgbbbgbbbbgrrrrrrrrrrK.",
    ".KrrrrgrrrrrgbbbbbbbbgrrrrrrrrrrK.",
    ".KrrrgKgrrrrgbbbgbbbbgrrrrrrrrrrK.",
    ".KrrrrgrrrrrgbbbbbbbbgrrrrrrrrrrK.",
    ".KrrrrKrrrrrgbbbgbbbbgrrrrrrrrrrK.",
    ".KrrKsKsKrrrgbbbbbbbbgrrrrrrrrrrK.",
    ".KrrKssKrrrrgbbbgbbbbgrrrrrrrrrrK.",
    ".KrrrKKrrrrrgbbbbbbbbgrrrrrrrrrrK.",
    ".KrrrrrrrrrrgbbbgbbbbgrrrrrrrrrrK.",
    ".KrrrrrrrrrrgbbbbbbbbgrrrrrrrrrrK.",
]
JACK = [
    "........................gg........",
    ".......................ggK........",
    "...........KKKKKKKKKKKggK.........",
    "..........KbbbbbbbbbbgbK..........",
    ".........KbbbbbbbbbbbbbbK.........",
    "........KKKKKKKKKKKKKKKKKK........",
    "..........KhhsssssssshhK..........",
    "..........KhsKKssssKKshK..........",
    "..........KhssKssssKsshK..........",
    "..........KssssssssssssK..........",
    "..........KssssssKKssssK..........",
    "...........KssssssssssK...........",
    "...........KsssKKKKsssK...........",
    "............KssssssssK............",
    "....KKKKKKKKKKssssssKKKKKKKKKK....",
    "...KrrrrrrrrrK......KrrrrrrrrrK...",
    "..KrrrrrrrrrrrK....KrrrrrrrrrrrK..",
    ".KrrrrrrrrrrrrrKKKKrrrrrrrrrrrrrK.",
    ".KrrrrrrrrrrrrrgbbgrrrrrrrrrrrrrK.",
    ".KrrrrrrrrrrrrrgbbgrrrrrrrrrrrrrK.",
    ".KbbbbKrrrrrrrrgbbgrrrrrrrrKbbbbK.",
    ".KbbbbKrrrrrrrrgbbgrrrrrrrrKbbbbK.",
    ".KKKKKKrrrrrrrrgbbgrrrrrrrrKKKKKK.",
    ".KsssKrrrrrrrrrgbbgrrrrrrrrrKsssK.",
    ".KKKKKrrrrrrrrrgbbgrrrrrrrrrKKKKK.",
    ".KrrrrrrrrrrrrrgbbgrrrrrrrrrrrrrK.",
    ".KrrrrrrrrrrrrrgbbgrrrrrrrrrrrrrK.",
]
COURT = {10: JACK, 11: QUEEN, 12: KING}
CODE = {'r': RED, 'b': BLUE, 'g': GOLD, 's': SKIN, 'h': HAIR}


class Pic:
    def __init__(self, bg=FACE, row=ROW_RED):
        self.ink = [[0] * W for _ in range(H)]
        self.col = [[bg if card_inside(x, y) else FELT for x in range(W)] for y in range(H)]
        self.bg = bg
        self.row = row

    def set(self, x, y, v=1):
        if 0 <= x < W and 0 <= y < H:
            self.ink[y][x] = v

    def paint(self, x, y, c):
        if 0 <= x < W and 0 <= y < H:
            self.col[y][x] = c

    def blit(self, bm, x0, y0, flip=False, scale=1, dotted=False, colour=None):
        """a bitmap in ink, or (colour) in a colour of the tiles"""
        h, w = len(bm), len(bm[0])
        for j in range(h):
            for i in range(w):
                if not bm[j][i]:
                    continue
                for b in range(scale):
                    for a in range(scale):
                        x, y = i * scale + a, j * scale + b
                        if flip:
                            x, y = w * scale - 1 - x, h * scale - 1 - y
                        if dotted and (x0 + x + y0 + y) % 2:
                            continue
                        if colour is None:
                            self.set(x0 + x, y0 + y)
                        else:
                            self.paint(x0 + x, y0 + y, colour)

    def outline(self, dotted=False):
        for y in range(H):
            for x in range(W):
                if not card_inside(x, y):
                    continue
                edge = x in (0, W - 1) or y in (0, H - 1) or not all(
                    card_inside(x + dx, y + dy) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
                if edge and (not dotted or (x + y) % 2 == 0):
                    self.ink[y][x] = 1

    def rotated_copy(self, x0, y0, x1, y1):
        """the area [x0, x1) x [y0, y1) again, turned half round (ink and colour)"""
        for y in range(y0, y1):
            for x in range(x0, x1):
                if self.ink[y][x]:
                    self.ink[H - 1 - y][W - 1 - x] = 1
                if self.col[y][x] != self.bg and card_inside(x, y):
                    self.col[H - 1 - y][W - 1 - x] = self.col[y][x]

    def cols(self):
        out = bytearray()
        for c in range(7):
            for y in range(H):
                b = 0
                for k in range(8):
                    b = b << 1 | self.ink[y][c * 8 + k]
                out.append(b)
        return out


def face(suit, rank, mono=False):
    red = suit >= 2
    colour = RED if red and not mono else None
    p = Pic(row=ROW_BLACK if not red and rank >= 10 else ROW_RED)
    p.outline()
    p.blit(bits(GLYPHS[RANKS[rank]]), 2, 2, colour=colour)
    p.blit((SMALL_OUT if mono else SMALL)[suit], 9, 2, colour=colour)
    p.rotated_copy(0, 0, 17, 10)
    pip = (PIP_OUT if mono else PIP)[suit]
    if rank == 0:
        p.blit((BIG_OUT if mono else BIG)[suit], 28 - 14, 40 - 14, colour=colour)
    elif rank < 10:
        for col, f in PIPS[rank + 1]:
            cy = round(Y0 + f * (Y1 - Y0))
            p.blit(pip, LX[col] - 4, cy - 4, flip=cy > 40, colour=colour)
    else:
        for x in range(10, 46):
            p.set(x, 12)
            p.set(x, 67)
        for y in range(12, 68):
            p.set(10, y)
            p.set(45, y)
        for j, line in enumerate(COURT[rank]):
            assert len(line) == 34, (rank, j, len(line))
            for i, ch in enumerate(line):
                x, y = 11 + i, 13 + j
                if ch == 'K':
                    p.set(x, y)
                    p.set(W - 1 - x, H - 1 - y)
                elif ch != '.':
                    p.paint(x, y, CODE[ch])
                    p.paint(W - 1 - x, H - 1 - y, CODE[ch])
    return p


def back():
    p = Pic(bg=BACK)
    p.outline()
    for y in range(H):
        for x in range(W):
            if not card_inside(x, y):
                continue
            inset = min(x, y, W - 1 - x, H - 1 - y)
            dx, dy = x + 0.5 - 28, y + 0.5 - 40
            r = (dx * dx + dy * dy) ** 0.5
            if inset == 3:
                p.set(x, y)
            elif inset >= 5 and r > 12.5:
                if (x + y) % 6 == 0 or (x - y) % 6 == 0:
                    p.set(x, y)
            # the emblem: a sun, gold, eight rays
            if r <= 5.3:
                p.paint(x, y, GOLD)
                if r > 4.3:
                    p.set(x, y)
            elif 7 <= r <= 11:
                a = (math.degrees(math.atan2(dy, dx)) + 360 + 22.5) % 45 - 22.5
                if abs(math.radians(a)) * r < 1.6 - (r - 7) * 0.25:
                    p.paint(x, y, GOLD)
    return p


def place(kind, suit=0):
    p = Pic(bg=SLOT)
    p.outline(dotted=True)
    if kind == 'found':
        p.blit(MIDO[suit], 28 - 10, 40 - 10)
    elif kind == 'king':
        p.blit(bits(GLYPHS['K']), 28 - 6, 40 - 7, scale=2, dotted=True)
    elif kind == 'redeal':
        for y in range(H):
            for x in range(W):
                d = ((x - 27.5) ** 2 + (y - 39.5) ** 2) ** 0.5
                if 9.5 <= d <= 12 and not (x > 28 and y < 36):
                    p.set(x, y)
        for i in range(6):          # the arrow's head
            for k in range(-i, i + 1):
                p.set(30 + i, 27 + k)
    return p


def masks():
    """the pixels outside the rounded corners: (column, row) -> bits"""
    m = {}
    for c in (0, 6):
        for y in list(range(R)) + list(range(H - R, H)):
            b = 0
            for k in range(8):
                b = b << 1 | (not card_inside(c * 8 + k, y))
            m[(c, y)] = b
    return m


# --------------------------------------------------------------- the tiles
class Tiles:
    """the tile set: each tile 64 colour indices; one tile, or a corner's
    three (outside: the felt, a face, the back), kept once under flips"""

    def __init__(self):
        self.tiles = []
        self.index = {}

    @staticmethod
    def flipped(t, hf, vf):
        rows = [t[r * 8:r * 8 + 8] for r in range(8)]
        rows = rows[::-1] if vf else rows
        return tuple(v for r in rows for v in (r[::-1] if hf else r))

    def add(self, group):
        """a group of tiles (1 or 3) -> (first tile number, hflip, vflip)"""
        for hf in (0, 1):
            for vf in (0, 1):
                key = tuple(self.flipped(g, hf, vf) for g in group)
                if key in self.index:
                    return self.index[key], hf, vf
        n = TILE0 + len(self.tiles)
        self.tiles += list(group)
        self.index[tuple(group)] = n
        return n, 0, 0

    def solid(self, c):
        return self.add([tuple([c] * 64)])[0]


def cell_tile(p, cx, cy, outside=None):
    """card (cx, cy) of picture p as 64 colour indices: under the ink the
    card's most frequent colour (the ink hides it: fewer tiles)"""
    px = []
    for y in range(cy * 8, cy * 8 + 8):
        for x in range(cx * 8, cx * 8 + 8):
            if card_inside(x, y):
                px.append((p.col[y][x], p.ink[y][x]))
            else:
                px.append((outside if outside is not None else p.col[y][x], 0))
    seen = [c for c, i in px if not i]
    common = max(sorted(set(seen)), key=seen.count) if seen else p.bg
    return tuple(common if i else c for c, i in px)


CORNERS = ((0, 0), (6, 0), (0, 9), (6, 9))


def picture_map(p, tiles):
    out = bytearray()
    for cy in range(H // 8):
        for cx in range(W // 8):
            if (cx, cy) in CORNERS:
                n, hf, vf = tiles.add([cell_tile(p, cx, cy, o) for o in (FELT, FACE, BACK)])
            else:
                n, hf, vf = tiles.add([cell_tile(p, cx, cy)])
            out.append(n & 0xFF)
            out.append(p.row << 4 | vf << 3 | hf << 2 | n >> 8)
    return out


def rgb(c):
    return ((c >> 8) * 17, (c >> 4 & 15) * 17, (c & 15) * 17)


def main():
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    tiles = Tiles()
    t_felt = tiles.solid(FELT)
    t_face = tiles.solid(FACE)
    t_back = tiles.solid(BACK)
    t_slot = tiles.solid(SLOT)
    pics = [face(s, r) for s in range(4) for r in range(13)]
    pics.append(back())
    pics.append(place('empty'))
    pics += [place('found', s) for s in range(4)]
    pics.append(place('king'))
    pics.append(place('redeal'))
    ncolour = len(pics)
    pics += [face(s, r, mono=True) for s in (2, 3) for r in range(13)]
    maps = [picture_map(p, tiles) if i < ncolour else bytes(MAP_BYTES) for i, p in enumerate(pics)]
    ntiles = len(tiles.tiles)
    assert TILE0 + ntiles <= 480, 'too many tiles: %d' % ntiles
    # every picture again from its map and the tiles: its colours where no ink
    for p, mp in zip(pics[:ncolour], maps):
        for cy in range(H // 8):
            for cx in range(W // 8):
                lo, hi = mp[2 * (cy * 7 + cx)], mp[2 * (cy * 7 + cx) + 1]
                t = tiles.flipped(tiles.tiles[(lo | (hi & 3) << 8) - TILE0], hi >> 2 & 1, hi >> 3 & 1)
                for k in range(64):
                    x, y = cx * 8 + k % 8, cy * 8 + k // 8
                    if card_inside(x, y) and not p.ink[y][x]:
                        assert t[k] == p.col[y][x], (cx, cy)
    data = [p.cols() + m for p, m in zip(pics, maps)]
    nrec = (len(data) + PER_RECORD - 1) // PER_RECORD
    for r in range(nrec):
        with open(os.path.join(out, 'cards.%d' % (r + 1)), 'wb') as f:
            for d in data[r * PER_RECORD:(r + 1) * PER_RECORD]:
                f.write(d)
    # the tiles, 4 bpp (the left pixel the high nibble), for vr_vram_load
    raw = bytearray()
    for t in tiles.tiles:
        for k in range(0, 64, 2):
            raw.append(t[k] << 4 | t[k + 1])
    ntrec = (ntiles + TILES_PER_RECORD - 1) // TILES_PER_RECORD
    for r in range(ntrec):
        chunk = raw[r * TILES_PER_RECORD * 32:(r + 1) * TILES_PER_RECORD * 32]
        a = TILE_ADDR + (TILE0 + r * TILES_PER_RECORD) * 32
        with open(os.path.join(out, 'tiles.%d' % (r + 1)), 'wb') as f:
            f.write(bytes([a & 0xFF, a >> 8 & 0xFF, a >> 16, len(chunk) & 0xFF, len(chunk) >> 8]))
            f.write(chunk)
            f.write(bytes(5))
    with open(os.path.join(out, 'tiles.bin'), 'wb') as f:     # (the host's)
        f.write(raw)
    m = masks()
    with open(os.path.join(out, 'cards.h'), 'w') as f:
        f.write('/* made by tools/mkcards.py */\n')
        f.write('#define PIC_BACK 52\n#define PIC_EMPTY 53\n#define PIC_FOUND 54\n'
                '#define PIC_KING 58\n#define PIC_REDEAL 59\n#define PIC_MONO 60\n#define PICS %d\n'
                '#define PIC_COLOUR %d\n' % (len(pics), ncolour))
        f.write('#define PIC_RECORDS %d\n#define PIC_PER_RECORD %d\n#define PIC_BYTES %d\n'
                '#define PIC_REC %d\n' % (nrec, PER_RECORD, INK_BYTES, PIC_REC))
        f.write('#define TILE_RECORDS %d\n#define NTILES %d\n#define TILE0 %d\n' % (ntrec, ntiles, TILE0))
        f.write('#define T_FELT %d\n#define T_FACE %d\n#define T_BACK %d\n#define T_SLOT %d\n'
                % (t_felt, t_face, t_back, t_slot))
        f.write('#define ROW_RED %d\n#define ROW_BLACK %d\n' % (ROW_RED, ROW_BLACK))
        # the map entries of the plain cards
        for name, t in (('FELT', t_felt), ('FACE', t_face), ('BACK', t_back), ('SLOT', t_slot)):
            f.write('#define E_%s 0x%04X\n' % (name, (t & 0xFF) | (ROW_RED << 4 | t >> 8) << 8))
        for name, tab in (('LIGHT', LIGHT), ('DARK', DARK)):
            for row in (ROW_RED, ROW_BLACK):
                f.write('#define PAL%d_%s {%s}\n' % (row, name, ', '.join(
                    '0x%03X' % v for v in palette_row(tab, row))))
        f.write('#define CORNER_ROWS %d\n' % R)
        for c in (0, 6):
            f.write('#define MASK_TOP%d {%s}\n' % (c, ', '.join('0x%02X' % m[(c, y)] for y in range(R))))
            f.write('#define MASK_BOT%d {%s}\n' % (c, ', '.join('0x%02X' % m[(c, y)] for y in range(H - R, H))))
    with open(os.path.join(out, 'cards.inc'), 'w') as f:     # (blit.s)
        f.write('; made by tools/mkcards.py\nT_FELT = %d\nT_BACK = %d\n' % (t_felt, t_back))
    print('%d pictures in %d records, %d tiles (%d to %d) in %d records' %
          (len(pics), nrec, ntiles, TILE0, TILE0 + ntiles - 1, ntrec))
    try:
        from PIL import Image
    except ImportError:
        return
    cols = 15
    rows = (len(pics) + cols - 1) // cols
    half_w = cols * (W + 4) + 4
    sheet = Image.new('RGB', (2 * half_w, rows * (H + 4) + 4))
    px = sheet.load()
    for half, (tab, ink, paper) in enumerate(((LIGHT, INK_LIGHT, PAPER_LIGHT), (DARK, INK_DARK, PAPER_DARK))):
        pal = {r: palette_row(tab, r) for r in (ROW_RED, ROW_BLACK)}
        ox = half * half_w
        for y in range(sheet.height):
            for x in range(half_w):
                px[ox + x, y] = rgb(tab[FELT])
        for i, p in enumerate(pics):
            x0, y0 = ox + 4 + (i % cols) * (W + 4), 4 + (i // cols) * (H + 4)
            for y in range(H):
                for x in range(W):
                    if i >= ncolour:
                        c = ink if p.ink[y][x] else (paper if card_inside(x, y) else tab[FELT])
                    elif p.ink[y][x]:
                        c = ink
                    else:
                        c = pal[p.row][p.col[y][x]]
                    px[x0 + x, y0 + y] = rgb(c)
    sheet = sheet.resize((sheet.width * 2, sheet.height * 2), Image.NEAREST)
    sheet.save(os.path.join(out, 'cards.png'))


if __name__ == '__main__':
    main()
