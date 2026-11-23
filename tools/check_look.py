#!/usr/bin/env python3
"""Veranda Solitaire in Veranda Look 3 "Sunroom" (Veranda's DECISIONS
V105, V132): the colours of a screen.

    check_look.py look DUMP MAP CARDS
    check_look.py tile CVT TILES NAME

DUMP: an x16emu dump (-dump V or RV). Checked: layer 0 shown in tile mode
(the SDK's vr_pic); palette entries 0-15 Sunroom's light ones (no
settings file), palette rows 2 and 3 the cards' light ones (CARDS:
tools/mkcards.py's folder, its cards.h); the tiles from 16 on its
tiles.bin; the menu bar's cards (rows 0-1) the kernel's, tile 2 (the
chrome) and the Veranda mark's two tile 11 (the cyan); the status bar's
(rows 58-59) tile 3, the theme's mist; every card of the table's rows
(2-57) the entry of MAP (host/soltool render's OUT.map: the state drawn
from scratch, its corners chosen as draw.c chooses them).

tile: the CVT carries a tile (its info block's flags bit 1; the tile
block in its first block: the colour at 96, the 32 x 32 bitmap at 97-224,
Veranda's docs/APPFORMAT.md) equal to the tile NAME of Veranda's
shell/tiles.txt (TILES), the one the shell shows for the class.
GPL-3.0-or-later (see COPYING).
"""
import sys

VDUMP = 132878
VRAM, COMPOSER = 131072, 256
MAP = 0x1AC00
LIGHT = [0xFFF, 0x223, 0xDDE, 0xEEF, 0x9CF, 0xACE, 0xFD5, 0xF87, 0x8D9, 0xB9F, 0xFA5, 0x6CE, 0x36A, 0x8BE,
         0x7A8, 0xDAC]
CHROME, CYAN, MIST = 2, 11, 3
TILES, PALETTE = 0x13000, 0x1FA00
ROW0, ROW1, STATUS = 2, 58, 58


def fail(m):
    print('FAIL ', m)
    sys.exit(1)


COLOURS = ['paper', 'ink', 'chrome', 'sidebar', 'select', 'title', 'yellow', 'red', 'green', 'purple',
           'orange', 'cyan', 'sky', 'sky2', 'hill', 'dusk']


def tile(cvt, tiles, name):
    """the shell's tile NAME (its colour, its 128 bytes) against the CVT's"""
    rows, colour, cur = [], None, None
    for line in open(tiles, encoding='utf-8'):
        w = line.split(None, 1)
        if w and w[0] == 'tile':
            cur = w[1].strip()
            continue
        if cur != name:
            continue
        if w and w[0] == 'colour':
            colour = COLOURS.index(w[1].strip())
        elif len(line.rstrip()) == 32 and not set(line.rstrip()) - set('#.'):
            rows.append(line.rstrip())
    if len(rows) != 32 or colour is None:
        fail('no tile "%s" in %s' % (name, tiles))
    want = bytes(sum(0x80 >> i for i, ch in enumerate(r[x:x + 8]) if ch == '#') for r in rows for x in range(0, 32, 8))
    d = open(cvt, 'rb').read()
    if not d[254 + 141] & 2:
        fail('%s: no tile block (flags $%02X)' % (cvt, d[254 + 141]))
    if d[96] != colour or d[97:225] != want:
        fail('%s: its tile (colour %d) is not the shell\'s "%s" (colour %d)' % (cvt, d[96], name, colour))
    print('ok    the CVT\'s tile is the shell\'s "%s" (%s, %d ink pixels)' % (name, COLOURS[colour],
                                                                         sum(bin(b).count('1') for b in want)))


def main():
    if len(sys.argv) == 5 and sys.argv[1] == 'tile':
        return tile(*sys.argv[2:])
    if len(sys.argv) != 5 or sys.argv[1] != 'look':
        sys.exit(__doc__)
    d = open(sys.argv[2], 'rb').read()
    v = d[len(d) - VDUMP:]
    if not v[VRAM] & 0x10:
        fail('layer 0 (the tiles) is not shown')
    p = v[VRAM + COMPOSER:]
    pal = [p[2 * i] | p[2 * i + 1] << 8 for i in range(16)]
    if pal != LIGHT:
        fail('palette entries 0-15 %s: want Sunroom\'s %s' % (' '.join('$%03X' % c for c in pal),
                                                            ' '.join('$%03X' % c for c in LIGHT)))
    h = {}
    for line in open(sys.argv[4] + '/cards.h'):
        w = line.split(None, 2)
        if len(w) == 3 and w[0] == '#define':
            h[w[1]] = w[2].strip()
    for row in (2, 3):
        want = [int(x, 16) for x in h['PAL%d_LIGHT' % row].strip('{}').split(',')]
        got = [p[2 * (16 * row + i)] | p[2 * (16 * row + i) + 1] << 8 for i in range(16)]
        if got[1:] != want[1:]:
            fail('palette row %d %s: want the cards\' %s' % (row, ' '.join('$%03X' % c for c in got),
                                                             ' '.join('$%03X' % c for c in want)))
    tiles = open(sys.argv[4] + '/tiles.bin', 'rb').read()
    t0 = int(h['TILE0'])
    if v[TILES + 32 * t0:TILES + 32 * t0 + len(tiles)] != tiles:
        fail('the cards\' tiles (%d from %d) are not in VRAM' % (len(tiles) // 32, t0))

    def entry(x, y):
        return v[MAP + 2 * (y * 128 + x)] | v[MAP + 2 * (y * 128 + x) + 1] << 8

    for y in range(2):
        for x in range(80):
            want = CYAN if x < 2 else CHROME
            if entry(x, y) != want:
                fail('menu bar card %d,%d: entry $%04X, want $%04X' % (x, y, entry(x, y), want))
    for y in range(STATUS, 60):
        for x in range(80):
            if entry(x, y) != MIST:
                fail('status bar card %d,%d: entry $%04X, want $%04X (the mist)' % (x, y, entry(x, y), MIST))
    m = open(sys.argv[3], 'rb').read()
    felt = int(h['E_FELT'], 16)
    bad, nfelt, ncard = [], 0, 0
    for y in range(ROW0, ROW1):
        for x in range(80):
            k = 2 * (y * 80 + x)
            want = m[k] | m[k + 1] << 8
            if entry(x, y) != want:
                bad.append((x, y, entry(x, y), want))
            elif want == felt:
                nfelt += 1
            else:
                ncard += 1
    if bad:
        for x, y, c, w in bad[:6]:
            print('      card %d,%d: entry $%04X, want $%04X' % (x, y, c, w))
        fail('%d cards of the table wrong' % len(bad))
    print('ok    the bar the kernel\'s (chrome, the mark cyan), the status bar mist, palette entries 0-15 '
          'Sunroom\'s, rows 2-3 and %d tiles the cards\'; the table %d cards of felt, %d of cards, each '
          'the state\'s' % (len(tiles) // 32, nfelt, ncard))


if __name__ == '__main__':
    main()
