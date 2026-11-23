#!/usr/bin/env python3
"""Veranda Solitaire's screens against the host's drawing of the state.

    check_screen.py same DUMP RENDER [PNG]     the play area equal, pixel for pixel
    check_screen.py outline DUMP RENDER [X Y]   the difference is a dragged outline:
                                                two frames, 56 wide, inverted (with
                                                X Y: its corner there)
    check_screen.py box DUMP RENDER             a dialog box is shown (its frame,
                                                140-499 x 130-251)

DUMP: an x16emu dump (-dump V or RV), or a SCREENn of the test build;
RENDER: host/soltool's render (80
bytes a row, 480 rows: VERA's foreground). PNG: the screen and the
drawing side by side, the differences marked. GPL-3.0-or-later (see COPYING).
"""
import sys

VDUMP = 132878
TOP, BOTTOM = 17, 464          # the play area (src/layout.h: PLAY_TOP, PLAY_BOTTOM)


def fore(path):
    """the foreground of a dump, or a screen saved by the test build (38,400 bytes)"""
    d = open(path, 'rb').read()
    if len(d) == 38400:
        return d
    return d[len(d) - VDUMP:][:38400]


def px(buf, x, y):
    return buf[y * 80 + x // 8] >> (7 - x % 8) & 1


def diffs(a, b):
    out = []
    for y in range(TOP, BOTTOM):
        for c in range(80):
            x = a[y * 80 + c] ^ b[y * 80 + c]
            if x:
                for k in range(8):
                    if x >> (7 - k) & 1:
                        out.append((c * 8 + k, y))
    return out


def png(a, b, d, path):
    from PIL import Image
    img = Image.new('RGB', (1280 + 8, 480), (128, 128, 128))
    for y in range(480):
        for x in range(640):
            img.putpixel((x, y), (0, 0, 0) if px(a, x, y) else (255, 255, 255))
            img.putpixel((648 + x, y), (0, 0, 0) if px(b, x, y) else (255, 255, 255))
    for x, y in d:
        img.putpixel((x, y), (255, 0, 0))
    img.save(path)


def main():
    mode, a, b = sys.argv[1], fore(sys.argv[2]), open(sys.argv[3], 'rb').read()
    d = diffs(a, b)
    if mode == 'same':
        if len(sys.argv) > 4:
            png(a, b, d, sys.argv[4])
        if d:
            xs = [p[0] for p in d]
            ys = [p[1] for p in d]
            print('FAIL  %d pixels differ in %d-%d x %d-%d' % (len(d), min(xs), max(xs), min(ys), max(ys)))
            return 1
        print('ok    the play area equals the drawing (%d x %d pixels)' % (640, BOTTOM - TOP))
        return 0
    if mode == 'outline':
        if not d:
            print('FAIL  no difference: no outline')
            return 1
        xs = [p[0] for p in d]
        ys = [p[1] for p in d]
        x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
        frame = set()
        for inset in (0, 1):
            for x in range(x0 + inset, x1 + 1 - inset):
                frame.add((x, y0 + inset))
                frame.add((x, y1 - inset))
            for y in range(y0 + inset, y1 + 1 - inset):
                frame.add((x0 + inset, y))
                frame.add((x1 - inset, y))
        frame = {p for p in frame if TOP <= p[1] < BOTTOM}
        if set(d) != frame or x1 - x0 + 1 != 56:
            print('FAIL  the difference (%d pixels, %d-%d x %d-%d) is not a 56-wide outline' % (len(d), x0, x1, y0, y1))
            return 1
        if len(sys.argv) > 5:
            ex, ey = int(sys.argv[4]), int(sys.argv[5])
            if (x0, y0) != (ex, max(ey, TOP)):
                print('FAIL  the outline is at %d, %d, not at %d, %d (where the card was dragged)' % (x0, y0, ex, ey))
                return 1
        print('ok    an outline at %d, %d, 56 x %d, inverted over the table' % (x0, y0, y1 - y0 + 1))
        return 0
    if mode == 'box':
        top = all(px(a, x, 130) for x in range(141, 499))
        left = all(px(a, 140, y) for y in range(131, 251))
        print(('ok    ' if top and left else 'FAIL  ') + 'a box framed at 140, 130')
        return 0 if top and left else 1
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main())
