#!/usr/bin/env python3
"""Veranda Solitaire: every card of the game accounted for in a state.

    check_cards.py STATE...

STATE: src/dump.c's lines (a test build's STATEn, the host's expectN):
the header "game G deal N ...", then each stack. Every card of the
game's decks must be on exactly one stack (talon, waste, foundations,
rows, reserves): Klondike's 52, Spider's 104 of its suits.
GPL-3.0-or-later (see COPYING).
"""
import sys
from collections import Counter

RANKS = 'A23456789TJQK'
# src/games.c: the decks' suits (C S H D) by game
SUITS = {0: 'CSHD', 1: 'CSHD', 2: 'CSHD', 3: 'CCCC', 4: 'CCHH', 5: 'CSHD',
         6: 'CSHD', 7: 'CSHD', 8: 'CSHD', 9: 'CSHD'}
DECKS = {3: 2, 4: 2, 5: 2}


def check(path):
    lines = open(path, encoding='latin-1').read().split('\n')
    head = lines[0].split()
    if head[:1] != ['game']:
        return 'no header'
    g = int(head[1])
    want = Counter()
    for _ in range(DECKS.get(g, 1)):
        for s in SUITS[g]:
            for r in RANKS:
                want[r + s] += 1
    have = Counter()
    for ln in lines[1:]:
        if ':' not in ln:
            continue
        for c in ln.split(':', 1)[1].split():
            have[c.upper()] += 1
    if have != want:
        lost = want - have
        extra = have - want
        return '%d cards, not %d: lost %s, extra %s' % (sum(have.values()), sum(want.values()),
                                                     ' '.join(sorted(lost.elements())) or '-',
                                                     ' '.join(sorted(extra.elements())) or '-')
    return None


def main():
    bad = 0
    for p in sys.argv[1:]:
        e = check(p)
        if e:
            print('FAIL  %s: %s' % (p, e))
            bad += 1
    if bad:
        return 1
    print('ok    %d states, every card on one stack' % (len(sys.argv) - 1))
    return 0


if __name__ == '__main__':
    sys.exit(main())
