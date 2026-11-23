# Veranda Solitaire

A solitaire collection for Veranda, the GEOS-style desktop for the
Commander X16: 640x480, colour cards on a plain sage felt (Veranda Look 3
"Sunroom"), the mouse. The
rules are PySolFC's (https://github.com/shlomif/PySolFC), translated into
a small data-driven engine in C; the cards are drawn for this program.
GPL-3.0-or-later: see `COPYING` and `NOTICE`.

## The games

| Game | PySolFC's game (id) | Notes |
|---|---|---|
| Klondike | Klondike (2) | draw 1, unlimited redeals |
| Klondike (Draw 3) | Klondike by Threes (18) | options/draw 3 |
| FreeCell | FreeCell (8) | super moves; deal numbers are Microsoft's |
| Spider (1 suit) | Spider (1 Suit) (269) | a full suit goes home by itself |
| Spider (2 suits) | Spider (2 Suits) (270) | |
| Spider (4 suits) | Spider (11) | |
| Golf | Golf (36) | one round, queens on kings |
| Pyramid | Pyramid (38) | three rounds; pairs of 13, kings alone |
| Yukon | Yukon (19) | |
| Russian Solitaire | Russian Solitaire (20) | Yukon by suit (cheap: one field) |

A game is a row of `src/games.c`: the talon's kind (deals, rounds), the
foundations' and rows' classes, the rows' sequence and base rank, and the
deal as a short program (PySolFC's `startGame`). The classes are
PySolFC's stack classes: `basicAcceptsCards`, `acceptsCards`,
`canMoveCards`, `canDropCards`, `canFlipCard`, the talons' `dealCards`
and `canDealCards`, `autoPlay` (flip, drop, deal) and `isGameWon`
(`src/engine.c`, each part named after the Python it follows).

**Deal numbers.** A deal number gives PySolFC's deal of that number:
`Game.shuffle` with `LCRandom31` and `ms_rearrange` (pysol_cards), so a
number below 32000 is the same deal as in PySolFC, and FreeCell's deals
are Microsoft's (#1 and #617 checked). Numbers up to 2147483647 are
PySolFC's "ms" deals. A new deal picks a number from 1 to 32000.

## Playing

- **Drag** a card, or a run from a card down, with the mouse: an outline
  follows the pointer; release it where it should go (the stack nearest
  the card's corner takes it, in its region, as PySolFC's
  `getClosestStack`). A drop that the rules refuse leaves the cards where
  they were. A press that comes while a drag is on (its release was lost)
  gives the drag up first; so do the menus.
- **Click** the talon to deal (or to turn the waste over); a click on a
  Golf column plays its card; a click on a free Pyramid king removes it.
- **Double-click** a card to send it to a foundation.
- Face-down cards turn up by themselves, and an emptied waste gets the
  next card (PySolFC's defaults).
- When no move is left the status bar says **No moves left** instead of
  the cards left (Undo, Restart deal or New deal from there). A move is a
  card or a run onto a stack that takes it (not a whole column onto an
  empty one), a foundation's card back where the game allows it, a card
  to turn up or a deal; where the talon turns over without end
  (Klondike), dealing counts only while a card of the talon or the waste
  would go somewhere.

Menus (the kernel's menu bar: the Veranda mark, the clock and the
speaker at the right, the Veranda UI font; in the Veranda look a dotted
line between groups and each item's key at the right):

- **Solitaire**: About Veranda Solitaire; the desk accessories of the
  system folder.
- **Game**: New deal (Alt+N), Restart deal (Alt+R), Deal number...
  (Alt+D); Choose game, Statistics...; Quit.
- **Edit**: Undo (Alt+Z), Redo (Alt+Y): every move back to the deal, as
  far as memory goes (the moves are kept in RAM banks).
- **Options**: Auto-drop (cards go home as soon as they can; off, as in
  PySolFC), Draw 1 / Draw 3 (Klondike; the current Klondike deal is
  dealt again the other way).

**The look** (Veranda Look 3 "Sunroom", Veranda's DECISIONS V105, V132):
the cards are drawn for this program (`tools/mkcards.py`): hearts and
diamonds in red, clubs and spades in ink, a bold index of rank and suit
in the top 9 rows (it shows on a card half covered), simple court
figures of its own (a jack in a feathered cap, a queen with a flower, a
king with a crown and a beard, in the suit's colours: red and
blue for hearts and diamonds, blue and red for clubs and spades, with
gold and skin), a blue back with a dark lattice and a gold sun; the
table is a plain felt in Sunroom's sage, the empty places a darker sage
with a dotted outline; the status bar is the theme's mist with its text
in Gate Sans 9 (the UI font if the font is not installed). Dark mode (as
Preferences sets it) turns the faces slate with light ink and the felt a
dark green. The colour is VERA's tiles under the bitmap (the SDK's
`vrpic.h`, Veranda's tile mode): 169 tiles of the cards' own (the rotated
half of a card is the same tiles flipped), palette rows 2 and 3. A
picture starts on an 8 x 8 card of the screen: the rows of cards do, and
a fan's offsets are 8 and 16 pixels (a long column takes the face-down
cards to 4 and the face-up ones to 8, the first face-up card then on a
card's top). The program carries its tile (`veranda/tile.txt`, the
shell's for the class). The Classic theme gets none of the colours: the
cards in black and white (hearts and diamonds outlined), the table's
dotted pattern.

The status bar shows the game and deal, the moves, the time and the
cards not yet home (or "No moves left"). A win sends the cards bouncing off the foundations;
a click or a key stops them and shows the result. **Statistics** for each
game (played, won, the winning streak and the best, the best time and the
fewest moves) are kept in `Veranda Solitaire Stats` in Veranda's system
data folder (`/VERANDA/.SYSTEM`, or the root without `/VERANDA`), with the
last game and the options; the desk accessories are listed from
`/VERANDA/APPS` (Veranda's DECISIONS V71).

## Building

Needs cc65 (`CC65_HOME`, default `~/.local/share/cc65`), Python 3 (and
Pillow for the gate's pictures), gcc for the host tests, and Veranda's
SDK:

    (cd ~/Projects/geos && git archive HEAD) | tar -x -C build/vsrc   # (the gate does this)
    (cd build/vsrc && python3 tools/apigen.py api/veranda.api build/gen && python3 tools/kbuild.py)
    build/vsrc/tools/mksdk.sh $PWD/build/sdk
    make -C veranda SDK=$PWD/build/sdk           # build/veranda/Veranda Solitaire.cvt
    make -C veranda SDK=$PWD/build/sdk run       # in the emulator

Put `Veranda Solitaire.cvt` with Veranda's applications (the SD card's
`VERANDA/APPS` folder) and double-click it. It needs Veranda with API 5 (main
since DECISIONS V84: the Veranda menu bar, the colour map's buffer) and
512 KB (10 RAM banks for the overlay, the cards and their tiles, one more
for every 2,048 steps of the undo history).

## Tests and gates

`tools/gate_veranda.sh` runs them all (`SOL_SD=0` skips the SD card
image runs, `SOL_MOUSE=0` the real-mouse run):

- **H, the engine on the host** (gcc, the same C): every game's deals
  against `tools/pysol_ref.py`, an independent transcription of
  PySolFC's deal code; `host/tests.c`, each game's rules on chosen
  positions (moves accepted and refused as PySolFC's classes do), FreeCell
  deals #1 and #617 against Microsoft's, a deal number reproducing its
  layout, undo and redo, wins; FreeCell #1 and Klondike #1 solved by
  `test/freecell1.moves` and `test/klondike1.moves` (found by
  `host/fcsolve.c` and `host/klsolve.c`); random play of every game (40
  deals each: drags, clicks, double clicks, undo and redo): after every
  action each card on exactly one stack, the talons dealt out and turned
  over, undo back to the deal; "No moves left" on chosen positions
  (`sol_has_move`).
- **K, Klondike through the mouse**: `test/klondike.plan` made into a
  mouse script by `host/mkscript.c` (the host engine and the layout
  place every press and drop; the menus' places are the kernel's
  Veranda bar geometry in the Veranda UI font's widths): the deal number typed in, a player's drags, double
  clicks and deals, undo and redo, the menus by the mouse (about,
  auto-drop, draw 3, statistics, choose game, a desk accessory from the
  system folder); at each snapshot the program's state equals the host
  engine's and its screen the host's drawing of that state
  (`host/render.c`, from scratch), pixel for pixel; a screenshot in the
  middle of a drag (the outline and nothing else); at the end the colours
  (`tools/check_look.py`): Sunroom's palette, the cards' palette rows and
  tiles in VRAM, the bar's cards chrome and the mark's cyan, the status
  bar mist, every card of the table the tile map entry of the end state
  drawn afresh (`host/render.c`'s map: the felt, each card's tiles, the
  corners as the program chooses them); the same on an SD card image.
- **W and F, Klondike #1 and FreeCell #1 won through the mouse**: 159 and
  124 moves as drags, double clicks and talon clicks; won, the state the
  host's; the win's animation and box; the statistics file; the same on
  an SD card image.
- **O, Golf, Pyramid, Yukon and Spider through the mouse**: states and
  screens at each snapshot.
- **G, the waste drawn again**: Klondike #1, a deal and the waste's top
  card home, the waste from two cards to one (`test/waste.plan`): the
  card left is drawn whole (it was not: the card taken away kept its left
  side, corner and all, over it).
- **X, no moves left**: Golf #4's talon dealt out by 16 clicks; then,
  and not before, the status bar says No moves left.
- **R, the dragged outline at the right**: screenshots in the middle of
  drags from and to the right row (x 528): the outline across x 512 and
  all of it past, where the card was dragged. (The outline's x / 8 took
  only 9 of x's 10 bits: past x 512 the frame was drawn 512 pixels to
  the left, a dragged card seen in two parts, the user's report.)
- **L, every game's first layout**: the state the host's, the screen
  its drawing pixel for pixel, the colours as in K. The CVT carries the
  shell's tile for the class Solitaire.
- **M, the real mouse**: an x16emu with `-mousescript`
  (`~/Projects/x16-emulator-mouse`) and the R49 ROM: a card dragged
  through the ROM's PS/2 path; the state and screen after the drop; a
  whole session (`test/mouse-long.plan`, 40 actions as PS/2 packets),
  each move's screen its state drawn afresh.
- **N, a release never seen**: the main loop held (`SOL_SLOWTICK`) while
  the button goes up and down again (`test/missed-release.mouse`), so a
  press reaches the program while it drags (the script waits for a slow
  pass to end before the drag, and gives the release and the press in
  the next); the drag is given up and no outline stays on the table (on a real X16 one was left as a line
  through a foundation).
- **S, the talon and the whole width through the real mouse**: Klondike
  #1's talon clicked at its place until empty (23 deals), the empty
  talon clicked (the waste turned over, round 2), a deal again
  (`test/stock.plan`); 78 moves as drags onto every foundation and every
  row, the right ones past x 512, and talon clicks (`test/right.plan`).
  After every move the screen is its state drawn afresh (no outline left
  after a drop) and every state has its 52 cards (`tools/check_cards.py`,
  which checks every snapshot of the other runs too).
- **Q, deals again and again, then quit**: 120 new deals, restarts and
  draw 3 / draw 1 by the menus and deal numbers under R49
  (`test/deals.plan`); then the release build quits to the shell from an
  SD card image through the real mouse (`test/quit.plan`): no crash, the
  shell starts. (The frame counter's interrupt routine is turned off
  before `EnterDeskTop`: the kernel loads the shell over the program
  before it resets the interrupt's vectors, and on a real X16 the
  interrupt ran the shell's bytes: "BRK at $46D2 (RAM bank 0)".)

Timings (the emulator's cycle counter, 8 MHz): a move's redraw 19-72 ms
(the cards, ink on both screens and their map entries), the status
fields that changed 13-49 ms (with the search for a move left), one step of the dragged outline 5.3-8.4 ms
(both screens), the whole table (a new deal) 171-254 ms, a frame of the
win's animation 2.7 ms.

## Inside

| Path | What |
|---|---|
| `src/sol.h`, `engine.c`, `games.c` | the engine and the games (portable C) |
| `src/layout.c` | the screen layout: stacks, fans, hit tests, the closest stack |
| `src/dump.c` | the state as text (tests) |
| `veranda/main.c` | the start, the move log in RAM banks, the frame counter |
| `veranda/ui.c` | menus, the mouse, keys, dialog boxes, statistics, the win (overlay 1) |
| `veranda/draw.c` | the table: only what changed is drawn again, ink and tile map; the status bar |
| `veranda/tile.txt` | the program's tile for the shell (Veranda Look 2) |
| `veranda/blit.s` | VERA: cards from RAM banks, the table's fill, the dragged outline (both screens in tile mode) |
| `tools/mkcards.py` | the cards (56 x 80: the ink, the map, the tiles, the palette rows), drawn from geometry, pixel fonts and pixel art |
| `tools/check_look.py` | the gate's checks of Sunroom's colours and of the tile |
| `host/` | the host tools: tests, the solvers, the script maker, the renderer |
| `test/` | the move lists and the sessions' plans |

**Memory.** The program (engine, layout, drawing) is in main RAM; the
menus', dialogs' and statistics' code is overlay 1, run in place from
its RAM bank (mapped for good as `appBank`), with the engine's deal and
most of its search for a move left (overlay 1 is then 7,983 of its 8,192
bytes; main RAM ends at $744C, the test build's at $7EE0, under the C
stack at $8000); the 86 card pictures (52
faces, the back, the empty places, the hearts and diamonds again in black
and white), each its ink and its 70 map entries, are records 2-9, kept in
RAM banks by the loader; their tiles are record 10 (copied into VRAM at
the start); the move log takes RAM banks as it grows (2,048 atoms a
bank).

**Drawing.** In tile mode everything is drawn on both screens (menus
come back from the back screen; the Classic theme draws the foreground
only, and menus and dialog boxes keep what they cover). A card's map
entries follow its ink; a corner card's tile is one of three (its
rounded corner's outside the felt, a face or the back), chosen by the
map entry already there. The engine reports the lowest card each
change touches; that card's place down to the stack's old or new end is
filled with the table and the cards reaching into it are drawn again,
clipped (a covered card only down to where the next one covers it). A
card is copied from its RAM bank into VERA a column at a time with the
address stepping by 80; the side columns merge the screen under the
rounded corners. Timings are in the gate's output.
