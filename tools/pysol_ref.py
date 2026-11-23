#!/usr/bin/env python3
"""PySolFC's deals, transcribed: an independent model of what PySolFC does
for a game number, to check the engine's tables against.

    pysol_ref.py <game> <deal>      the layout after startGame, one stack a line

Written from reading pysollib/game/__init__.py (createCards, shuffle),
pysol_cards/random.py (LCRandom31, ms_rearrange, RandomBase.shuffle),
pysollib/stack.py (dealRow, dealToStacks, WasteTalonStack.dealCards,
FaceUpWasteTalonStack) and each game's startGame in pysollib/games/;
nothing of PySolFC is imported or run. Cards: rank AT..K and suit C S H D
(PySolFC's suit order); a face-down card in lower case.

GPL-3.0-or-later (see COPYING); derived from PySolFC (see NOTICE).
"""
import sys

RANKS = 'A23456789TJQK'
SUITS = 'CSHD'


class LCRandom31:
    MAX_SEED = (1 << 33) - 1

    def __init__(self, seed):
        self.seed = seed
        self.seedx = seed if seed < 0x100000000 else seed - 0x100000000

    def _rand(self):
        self.seedx = (self.seedx * 214013 + 2531011) & self.MAX_SEED
        return (self.seedx >> 16) & 0x7fff

    def random(self):
        ret = self._rand()
        return ret if self.seed < 0x80000000 else ret | 0x8000

    def randint(self, a, b):
        return a + self.random() % (b + 1 - a)

    def shuffle(self, seq):
        for n in range(len(seq) - 1, 0, -1):
            j = self.randint(0, n)
            seq[n], seq[j] = seq[j], seq[n]
        return seq


def ms_rearrange(cards):
    if len(cards) != 52:
        return cards
    c = []
    for i in range(13):
        for j in (0, 39, 26, 13):
            c.append(cards[i + j])
    return c


class Card:
    def __init__(self, suit, rank):
        self.suit, self.rank, self.face_up = suit, rank, False

    def __str__(self):
        s = RANKS[self.rank] + SUITS[self.suit]
        return s if self.face_up else s.lower()


class Game:
    def __init__(self, decks, suits, nrows, nfound, reserves=0, waste=True):
        self.cards = []
        for deck in range(decks):
            for suit in suits:
                for rank in range(13):
                    self.cards.append(Card(suit, rank))
        self.talon = []
        self.waste = [] if waste else None
        self.rows = [[] for _ in range(nrows)]
        self.foundations = [[] for _ in range(nfound)]
        self.reserves = [[] for _ in range(reserves)]

    def shuffle(self, seed):
        cards = list(self.cards)
        cards = ms_rearrange(cards)
        LCRandom31(seed).shuffle(cards)
        self.talon = cards           # the last card is the top

    # DealRow_StackMethods.dealRow / dealToStacks
    def dealRow(self, rows=None, flip=1, reverse=0):
        if rows is None:
            rows = self.rows
        if reverse:
            rows = list(reversed(rows))
        for r in rows:
            c = self.talon.pop()
            assert not c.face_up
            if flip:
                c.face_up = True
            r.append(c)

    # WasteTalonStack.dealCards (num_deal 1), the first card of a game
    def dealCards(self, faceup_talon=False, num_deal=1):
        for i in range(num_deal):
            c = self.talon.pop()
            c.face_up = True
            self.waste.append(c)
        if faceup_talon and self.talon:     # FaceUpWasteTalonStack.fillStack
            self.talon[-1].face_up = True

    def lines(self):
        out = ['talon: ' + ' '.join(map(str, self.talon))]
        if self.waste is not None:
            out.append('waste: ' + ' '.join(map(str, self.waste)))
        for i, f in enumerate(self.foundations):
            out.append('found%d: %s' % (i, ' '.join(map(str, f))))
        for i, r in enumerate(self.rows):
            out.append('row%d: %s' % (i, ' '.join(map(str, r))))
        for i, r in enumerate(self.reserves):
            out.append('reserve%d: %s' % (i, ' '.join(map(str, r))))
        return [l.rstrip() for l in out]


def klondike(num_deal):
    def f(seed):           # Klondike.startGame(flip=0, reverse=1)
        g = Game(1, (0, 1, 2, 3), 7, 4)
        g.shuffle(seed)
        for i in range(1, len(g.rows)):
            g.dealRow(rows=g.rows[i:], flip=0, reverse=1)
        g.dealRow(reverse=1)
        g.dealCards(num_deal=num_deal)     # KlondikeByThrees: num_deal=3
        return g
    return f


def freecell(seed):        # FreeCell.startGame
    g = Game(1, (0, 1, 2, 3), 8, 4, reserves=4, waste=False)
    g.shuffle(seed)
    for i in range(5):
        g.dealRow()
    g.dealRow()
    g.dealRow(rows=g.rows[:4])
    return g


def spider(suits):
    def f(seed):           # RelaxedSpider.startGame(flip=0)
        g = Game(2, suits, 10, 8, waste=False)
        g.shuffle(seed)
        for i in range(4):
            g.dealRow(flip=0)
        r = g.rows
        g.dealRow(rows=(r[0], r[3], r[6], r[9]), flip=0)
        g.dealRow()
        return g
    return f


def golf(seed):            # Golf.startGame(num_rows=5)
    g = Game(1, (0, 1, 2, 3), 7, 0)
    g.shuffle(seed)
    for i in range(4):
        g.dealRow()
    g.dealRow()
    g.dealCards()
    # the waste is Golf's foundation: shown as found0
    g.foundations = [g.waste]
    g.waste = None
    return g


def pyramid(seed):         # Pyramid.startGame
    g = Game(1, (0, 1, 2, 3), 28, 1)
    g.shuffle(seed)
    g.dealRow()
    g.dealCards(faceup_talon=True)
    return g


def yukon(seed):           # Yukon.startGame
    g = Game(1, (0, 1, 2, 3), 7, 4, waste=False)
    g.shuffle(seed)
    for i in range(1, len(g.rows)):
        g.dealRow(rows=g.rows[i:], flip=0)
    for i in range(4):
        g.dealRow(rows=g.rows[1:], flip=1)
    g.dealRow()
    return g


# the engine's game numbers (src/games.c)
GAMES = [klondike(1), klondike(3), freecell, spider((0, 0, 0, 0)), spider((0, 0, 2, 2)),
         spider((0, 1, 2, 3)), golf, pyramid, yukon, yukon]

if __name__ == '__main__':
    g = GAMES[int(sys.argv[1])](int(sys.argv[2]))
    print('\n'.join(g.lines()))
