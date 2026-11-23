#!/bin/bash
# Veranda Solitaire's gates, in the style of Veranda's tools/gate_paint.sh.
#
#   tools/gate_veranda.sh        SOL_SD=0: skip the SD card runs
#                                SOL_MOUSE=0: skip the real-mouse run
#
# Needs: Veranda (VERANDA=~/Projects/geos: its HEAD is exported into
# build/vsrc and built there, never in Veranda's tree; the SDK installed
# from it into build/sdk), cc65 (CC65_HOME), gcc, x16emu and its ROM
# (EMU, ROM), Python 3 with Pillow.
#
# H. the engine on the host (gcc, the same C as the X16 runs):
#    - each game's deals against tools/pysol_ref.py, a transcription of
#      PySolFC's deal code, for several deal numbers;
#    - host/tests.c: each game's rules on chosen positions (accepted or
#      refused, as PySolFC's classes do), Microsoft's FreeCell deals #1
#      and #617, a deal number giving the same layout again, undo and
#      redo, win detection, FreeCell #1 solved by test/freecell1.moves;
#      random play of every game (every card on one stack after each
#      action, the talons turned over); "No moves left" (sol_has_move)
# 1. the application against the installed SDK: an application, class
#    Solitaire, $0801, VRND, 640x480, API 5, banked overlays (the UI's
#    overlay and the card pictures) and a tile (the shell's for the class,
#    shell/tiles.txt), main RAM below the C stack; it starts through the
#    loader (AUTOSTART.cvt) and logs SOLITAIRE RUNNING; Veranda Look 3
#    "Sunroom" (tools/check_look.py): the palette Sunroom's, the cards'
#    palette rows and tiles (tools/mkcards.py) in VRAM, the kernel's menu
#    bar (its cards the chrome, the mark's the cyan), the status bar the
#    mist, every card of the table the tile map entry of the state drawn
#    afresh (host/soltool render's OUT.map: the felt, the cards' tiles)
# K. Klondike through the mouse (test/klondike.plan, made into a script
#    by host/mkscript with the host engine and layout): the deal number
#    typed (Alt + D), a player's 24 actions as drags, double clicks and
#    clicks on the talon, undo and redo (Alt + Z, Alt + Y), the about box
#    through the Solitaire menu (the kernel's bar), options/auto-drop and draw 3, statistics,
#    choose game, a desk accessory (Veranda's calculator, from the system
#    folder) through the Solitaire menu; the program's snapshots of the state
#    equal the host engine's; the screen at the end equals the host's
#    drawing of the state, pixel for pixel; a screenshot in the middle of
#    a drag (the outline and nothing else); the display (Veranda's
#    check_vera.py); the same on an SD card image (Veranda's vcard.py)
# W. Klondike #1 won through the mouse (test/klondike-win.plan): the 159
#    moves of host/klsolve.c's solution as drags, double clicks and talon
#    clicks; the state the host's (won); the win's animation and box; the
#    same on an SD card image
# F. FreeCell #1 won through the mouse (test/freecell.plan): 124 moves,
#    drags and double clicks; the state equals the host's (won); the win's
#    cards bouncing, a click, the win box; the same on an SD card image
# O. Golf, Pyramid, Yukon and Spider through the mouse (test/others.plan):
#    the state and the screen at each snapshot equal the host's
# G. the waste drawn again (the user's R49 report): Klondike #1, a deal,
#    the waste's top card home: from two cards to one, the one left must be
#    drawn whole (test/waste.plan)
# X. no moves left: Golf #4's talon dealt out, then the status bar says
#    so (test/nomoves.plan), not before
# R. the dragged outline at the right of the screen (the user's report: a
#    card dragged past x 512 drawn in two, its right part at the left):
#    screenshots in the middle of drags from and to the right row, the
#    outline where the card was dragged (test/right-shots.plan)
# L. each game's first layout: its screen against tools/check_screen.py's
#    drawing of the host engine's deal (pixel for pixel, the play area)
# M. the real mouse (x16emu with -mousescript, ~/Projects/x16-emulator-mouse,
#    R49's ROM): Klondike #1, a card dragged through the ROM's PS/2 path;
#    the program's state after the drop equals the host engine's; a whole
#    session (test/mouse-long.plan: 40 actions as PS/2 packets), each
#    move's screen its state drawn afresh, the end state the host's
# N. a release the program never sees (the user's R49 report: a line left
#    through a foundation): the test build with SOL_SLOWTICK holds the main
#    loop; test/missed-release.mouse lets the button go and press again
#    meanwhile, so a press comes while the program drags; no outline may
#    stay on the table
# S. the talon and the right of the table through the real mouse:
#    test/stock.plan (Klondike #1's talon clicked at its place until
#    empty, the empty talon clicked: the waste turned over, a deal again)
#    and test/right.plan (78 moves as drags onto every foundation and row,
#    x 48-583, and talon clicks to the talon's end); after every move the screen its state
#    drawn afresh (no outline left), every state with its 52 cards
#    (tools/check_cards.py, also for every snapshot of the other runs)
# Q. deals again and again (test/deals.plan: new deal, restart, draw 3 and
#    draw 1 by the menus, deal numbers) under R49; then the release build
#    quits to the shell from an SD card image (test/quit.plan, the real
#    mouse): the user's crash "BRK at $46D2 (RAM bank 0)"
# Timings (the emulator's cycle counter): a deal's whole table, a move's
# redraw, a drag's outline step.
set -e -o pipefail
cd "$(dirname "$0")/.."
TOP=$(pwd)
VERANDA=${VERANDA:-$HOME/Projects/geos}
EMU=${EMU:-$HOME/Projects/x16-emulator/build/x16emu}
ROM=${ROM:-$HOME/Projects/x16-rom/build/x16/rom.bin}
MEMU=${MEMU:-$HOME/Projects/x16-emulator-mouse/build/x16emu}
R49=${R49:-$HOME/Downloads/Release.R49.ROM.Image/rom.bin}
export CC65_HOME=${CC65_HOME:-$HOME/.local/share/cc65}
export PATH=$CC65_HOME/bin:$PATH
PY="python3 -I -B"
OUT=build/gate
SDK=$TOP/build/sdk
export EMU ROM SDK

ok() { echo "ok    $*"; }
fail() { echo "FAIL  $*" >&2; exit 1; }

rm -rf "$OUT"
mkdir -p "$OUT/host"

#---------------------------------------------------------------- H
echo "== H: the engine on the host"
H=$OUT/host
$PY tools/mkcards.py "$H/cards" || fail "the card pictures"
gcc -O2 -Wall -I"$H/cards" -o "$H/soltool" host/soltool.c host/render.c host/platform.c src/engine.c src/games.c \
	src/layout.c src/dump.c || fail "the host tool does not build"
gcc -O2 -Wall -o "$H/tests" host/tests.c host/platform.c src/engine.c src/games.c src/layout.c src/dump.c \
	|| fail "the host tests do not build"
gcc -O2 -Wall -o "$H/mkscript" host/mkscript.c host/platform.c src/engine.c src/games.c src/layout.c src/dump.c \
	|| fail "mkscript does not build"
n=0
for g in 0 1 2 3 4 5 6 7 8 9; do
	for d in 1 2 3 617 1000 11982 31999 123456789; do
		"$H/soltool" deal $g $d > "$H/deal.c.txt"
		$PY tools/pysol_ref.py $g $d > "$H/deal.py.txt"
		cmp -s "$H/deal.c.txt" "$H/deal.py.txt" || { diff "$H/deal.py.txt" "$H/deal.c.txt" | head; fail "game $g deal $d differs from PySolFC's deal code"; }
		n=$((n + 1))
	done
done
ok "H: $n deals (10 games x 8 numbers) equal tools/pysol_ref.py's transcription of PySolFC's"
"$H/tests" test/freecell1.moves test/klondike1.moves > "$H/tests.log" || { cat "$H/tests.log"; fail "the host tests"; }
ok "H: $(tail -1 "$H/tests.log" | sed 's/^ok *//') (rules, MS FreeCell #1 and #617, undo/redo, wins, FreeCell #1 and Klondike #1 solved)"

#---------------------------------------------------------------- 1
echo "== 1: the application"
rm -rf build/vsrc
mkdir -p build/vsrc
(cd "$VERANDA" && git archive HEAD) | tar -x -C build/vsrc
(cd build/vsrc && $PY tools/apigen.py api/veranda.api build/gen >/dev/null && $PY tools/kbuild.py >/dev/null) \
	|| fail "Veranda does not build (in build/vsrc)"
build/vsrc/tools/mksdk.sh "$SDK" >/dev/null || fail "the SDK does not install"
[ -f "$SDK/bin/VERANDA.PRG" ] || fail "the SDK has no VERANDA.PRG"
ok "Veranda $(cd "$VERANDA" && git rev-parse --short HEAD) built in build/vsrc, its SDK installed in build/sdk"
# the Veranda UI font, for the menus' geometry in the scripts (host/mkscript)
$PY build/vsrc/tools/mkfont.py --ui "$OUT/uifont.bin" >/dev/null || fail "the UI font"
export SOL_UIFONT=$TOP/$OUT/uifont.bin
make -s -C veranda SDK="$SDK" OUT="$TOP/$OUT/app" >"$OUT/app.log" 2>&1 || { cat "$OUT/app.log"; fail "Veranda Solitaire does not build"; }
A=$OUT/app/sol.cvt
field() { $PY "$SDK/tools/vpack.py" --list "$1" | sed -n "s/^$2 *//p"; }
[ "$(field "$A" 'geos type')" = 6 ] && [ "$(field "$A" class)" = 'Solitaire   V1.0' ] || fail "the CVT: type or class"
[ "$(field "$A" load)" = '$0801' ] && [ "$(field "$A" veranda)" = True ] && [ "$(field "$A" mode)" = 640x480 ] \
	&& [ "$(field "$A" api)" = 5 ] && [ "$(field "$A" flags)" = 3 ] || fail "the CVT: load address, VRND, mode, API, banked overlays or tile"
end=$(field "$A" end)
[ $((16#${end#\$})) -le $((16#8000)) ] || fail "the program ends at $end (the C stack is at \$8000)"
ok "1: an application, class Solitaire, \$0801-$end, VRND, 640x480, API 5, records $(field "$A" records)"
r=$($PY tools/check_look.py tile "$A" build/vsrc/shell/tiles.txt Solitaire) || { echo "$r"; fail "1: the tile"; }
ok "1: ${r#ok    } (the class's tile in shell/tiles.txt)"
tools/run.sh "$A" "$OUT/app/run" 30
grep -aq "SOLITAIRE RUNNING" "$OUT/app/run/console.txt" || fail "it does not start"
ok "1: it starts through the loader (AUTOSTART.cvt)"

# the session of plan $1 (game $2, deal $3 at the start) into $OUT/$4
session() {
	local plan=$1 g=$2 d=$3 n=$4 secs=$5
	local S=$OUT/$n
	mkdir -p "$S"
	"$H/mkscript" "$plan" "$S" 2>"$S/mkscript.log" || { cat "$S/mkscript.log"; fail "$n: the script"; }
	make -s -C veranda test SDK="$SDK" OUT="$TOP/$S" SOL_GAME=$g SOL_DEAL=$d >"$S/build.log" 2>&1 \
		|| { cat "$S/build.log"; fail "$n: the test build"; }
	tools/run.sh "$S/sol.cvt" "$S/run" "$secs"
	[ -f "$S/run/dump.bin" ] || { tail -5 "$S/run/console.txt"; fail "$n: the session did not finish"; }
}

# the snapshots of folder $2 against the host's in $1
states() {
	local e i=0
	for e in "$1"/expect*; do
		i=$((i + 1))
		[ -f "$2/STATE$i" ] || fail "$3: no STATE$i"
		cmp -s "$e" "$2/STATE$i" || { diff "$e" "$2/STATE$i"; fail "$3: snapshot $i differs from the host engine's"; }
		$PY tools/check_cards.py "$2/STATE$i" >/dev/null || { $PY tools/check_cards.py "$2/STATE$i"; fail "$3: snapshot $i: a card lost or doubled"; }
	done
	echo $i
}

# the screens the program saved with its snapshots ($2/SCREENn) against
# the host's drawings of the host engine's states ($1/expectn)
screens() {
	local e i=0 r
	for e in "$1"/expect*; do
		i=$((i + 1))
		"$H/soltool" render "$H/cards" "$e" "$1/render$i.bin" || fail "$3: the drawing $i"
		r=$($PY tools/check_screen.py same "$2/SCREEN$i" "$1/render$i.bin" "$1/cmp$i.png") \
			|| { echo "$r" >&2; fail "$3: the screen at snapshot $i is not the state drawn afresh"; }
	done
	echo $i
}

# the SD card: the session's CVT on an image, the files copied back
runsd() {
	local cvt=$1 d=$2 secs=$3
	shift 3
	mkdir -p "$d/card" "$d/out"
	cp "$cvt" "$d/card/AUTOSTART.cvt"
	for f in $EXTRA; do cp "$f" "$d/card/"; done
	cp "$SDK/bin/VERANDA.SYS" "$d/card/"
	$PY "$SDK/tools/vcard.py" make --mb 64 "$d/sd.img" "$d/card" >/dev/null || fail "vcard make"
	cp "$SDK/bin/VERANDA.PRG" "$d/"
	(cd "$d" && SDL_VIDEODRIVER=dummy timeout -s INT "$secs" "$EMU" -rom "$ROM" -sdcard sd.img -prg VERANDA.PRG \
		-run -warp -sound none -echo -dump V > out/console.txt 2>&1) || true
	[ -f "$d/dump.bin" ] || { tail -5 "$d/out/console.txt"; fail "SD card: the session did not finish"; }
	mv "$d/dump.bin" "$d/out/"
	for f in "$@"; do
		$PY "$SDK/tools/vcard.py" get "$d/sd.img" "$f" "$d/out/$f" >/dev/null || fail "SD card: no $f"
	done
}

# a screenshot of session $1 stopped at frame $3, as $OUT/$2.png
shot() {
	local n=$1 name=$2 frame=$3 g=$4 d=$5 plan=$6
	local T=$OUT/$n/shot-$name
	mkdir -p "$T"
	"$H/mkscript" "$plan" "$T" "$frame" 2>/dev/null || fail "$n: the script stopped at $frame"
	make -s -C veranda test SDK="$SDK" OUT="$TOP/$T" SOL_GAME=$g SOL_DEAL=$d >"$T/build.log" 2>&1 \
		|| { cat "$T/build.log"; fail "$n: the test build stopped at $frame"; }
	tools/run.sh "$T/sol.cvt" "$T/run" 120
	[ -f "$T/run/dump.bin" ] || fail "$n: no dump at frame $frame"
	$PY "$VERANDA/tools/check_vera.py" "$T/run/dump.bin" >/dev/null || fail "$name: the display"
	$PY "$SDK/tools/vram2png.py" --color "$T/run/dump.bin" "$OUT/$name.png" >/dev/null
}

# timings: "TIME what cycles": what: n, min-max ms
timings() {
	grep -a -o "TIME [A-Z]* [0-9A-F]*" "$1" | while read -r _ w c; do echo "$w $((16#$c))"; done |
		awk '{n[$1]++; v=$2/8000; if(!($1 in lo)||v<lo[$1])lo[$1]=v; if(v>hi[$1])hi[$1]=v}
		     END{for(k in n) printf "%s %d x %.1f-%.1f ms; ", k, n[k], lo[k], hi[k]}'
}

#---------------------------------------------------------------- K
echo "== K: Klondike through the mouse"
# a desk accessory for the Solitaire menu: Veranda's calculator
mkdir -p "$OUT/da"
make -s -C build/vsrc/apps/calc SDK="$SDK" OUT="$TOP/$OUT/da" >"$OUT/da.log" 2>&1 || { cat "$OUT/da.log"; fail "the calculator does not build"; }
export EXTRA=$TOP/$OUT/da/Calculator.cvt
session test/klondike.plan 0 5 klondike 120
K=$OUT/klondike
k=$(states "$K" "$K/run" K)
ok "K: deal #1 typed in, $(grep -c 'home\|->\|deal \*/' "$K/script.h") actions by drags, double clicks and clicks, undo and redo, the menus by the mouse (about, auto-drop, draw 3, statistics, choose game): the $k snapshots equal the host engine's"
$PY "$VERANDA/tools/check_vera.py" "$K/run/dump.bin" >/dev/null || fail "K: the display"
$PY "$SDK/tools/vram2png.py" --color "$K/run/dump.bin" "$OUT/klondike-end.png" >/dev/null
"$H/soltool" render "$H/cards" "$K/expect$k" "$K/end.bin" || fail "K: the drawing"
r=$($PY tools/check_screen.py same "$K/run/dump.bin" "$K/end.bin" "$K/cmp.png") || { echo "$r"; fail "K: the screen at the end is not the state drawn afresh"; }
ok "K: after the session (every redraw partial) the play area equals the end state drawn from scratch, pixel for pixel"
r=$($PY tools/check_look.py look "$K/run/dump.bin" "$K/end.bin.map" "$H/cards") || { echo "$r"; fail "K: the colours after the session (felt, bar, status bar)"; }
ok "K: after the session (every redraw partial, a desk accessory on the way): ${r#ok    }"
k=$(screens "$K" "$K/run" K)
ok "K: at each of the $k snapshots too: the screen the program saved equals the host's drawing of the state"
grep -aq "SOL NO" "$K/run/console.txt" && fail "K: a missing overlay"
f=$(sed -n 's|^/\* shot drag \([0-9]*\) \*/|\1|p' "$K/script.h")
shot klondike klondike-drag "$f" 0 5 test/klondike.plan
"$H/soltool" render "$H/cards" "$K/shot-drag.state" "$K/shot-drag.bin" || fail "K: the drawing"
r=$($PY tools/check_screen.py outline "$K/shot-klondike-drag/run/dump.bin" "$K/shot-drag.bin") || { echo "$r"; fail "K: the drag's screenshot"; }
ok "K: in the middle of a drag the screen is the table and ${r#ok    } (klondike-drag.png)"
f=$(sed -n 's|^/\* shot about \([0-9]*\) \*/|\1|p' "$K/script.h")
shot klondike klondike-about "$f" 0 5 test/klondike.plan
$PY tools/check_screen.py box "$OUT/klondike/shot-klondike-about/run/dump.bin" "$K/shot-drag.bin" >/dev/null || fail "K: the about box is not shown"
ok "K: the about box through the Solitaire menu (klondike-about.png)"
f=$(sed -n 's|^/\* shot stats \([0-9]*\) \*/|\1|p' "$K/script.h")
shot klondike klondike-stats "$f" 0 5 test/klondike.plan
$PY tools/check_screen.py box "$OUT/klondike/shot-klondike-stats/run/dump.bin" "$K/shot-drag.bin" >/dev/null || fail "K: the statistics box is not shown"
ok "K: Game/Statistics... shows the box (klondike-stats.png)"
grep -aq "CALC RUNNING" "$K/run/console.txt" && grep -aq "CALC CLOSED" "$K/run/console.txt" || fail "K: Solitaire/Calculator did not run"
ok "K: Solitaire/Calculator (a desk accessory of the system folder) ran and closed; the table came back"
echo "      timings: $(timings "$K/run/console.txt")"
if [ "${SOL_SD:-1}" != 0 ]; then
	runsd "$K/sol.cvt" "$K/sd" 120 $(cd "$K" && ls expect* | sed 's/expect/STATE/')
	states "$K" "$K/sd/out" "K (SD card)" >/dev/null
	ok "K on an SD card image: the same snapshots"
fi

#---------------------------------------------------------------- W
echo "== W: Klondike #1 won through the mouse"
export EXTRA=
session test/klondike-win.plan 0 1 kwin 200
W=$OUT/kwin
states "$W" "$W/run" W >/dev/null
grep -q "^game 0 deal 1 round 2 moves 159 won 1" "$W/run/STATE1" || fail "W: not won in 159 moves"
grep -aq "SOL WON" "$W/run/console.txt" && grep -aq "SOL WIN BOX" "$W/run/console.txt" || fail "W: the win was not shown"
ok "W: Klondike #1, 159 moves ($(grep -c '^m' test/klondike1.moves) drags and double clicks, $(grep -c '^c t' test/klondike1.moves) talon clicks): won, the state the host engine's; the win's animation and box"
$PY "$SDK/tools/vram2png.py" --color "$W/run/dump.bin" "$OUT/klondike-win.png" >/dev/null
$PY tools/check_screen.py box "$W/run/dump.bin" "$W/run/dump.bin" >/dev/null || fail "W: the win box is not shown"
f=$(sed -n 's|^/\* shot kwinanim \([0-9]*\) \*/|\1|p' "$W/script.h")
shot kwin klondike-winanim "$f" 0 1 test/klondike-win.plan
f=$(sed -n 's|^/\* shot kwdrag \([0-9]*\) \*/|\1|p' "$W/script.h")
shot kwin klondike-windrag "$f" 0 1 test/klondike-win.plan
"$H/soltool" render "$H/cards" "$W/shot-kwdrag.state" "$W/shot-kwdrag.bin" || fail "W: the drawing"
r=$($PY tools/check_screen.py outline "$W/shot-klondike-windrag/run/dump.bin" "$W/shot-kwdrag.bin") || { echo "$r"; fail "W: the drag's screenshot"; }
ok "W: screenshots klondike-windrag.png (${r#ok    }), klondike-winanim.png, klondike-win.png"
echo "      timings: $(timings "$W/run/console.txt")"
if [ "${SOL_SD:-1}" != 0 ]; then
	runsd "$W/sol.cvt" "$W/sd" 200 STATE1
	states "$W" "$W/sd/out" "W (SD card)" >/dev/null
	ok "W on an SD card image: won, the same state"
fi

#---------------------------------------------------------------- F
echo "== F: FreeCell #1 won through the mouse"
export EXTRA=
session test/freecell.plan 2 1 freecell 200
F=$OUT/freecell
k=$(states "$F" "$F/run" F)
grep -q "^game 2 deal 1 round 1 moves 124 won 1" "$F/run/STATE1" || fail "F: not won in 124 moves"
grep -aq "SOL WON" "$F/run/console.txt" && grep -aq "SOL WIN BOX" "$F/run/console.txt" || fail "F: the win was not shown"
ok "F: 124 moves (72 drags, 52 double clicks): won, the state equals the host engine's; the win's animation and box"
$PY "$SDK/tools/vram2png.py" --color "$F/run/dump.bin" "$OUT/freecell-win.png" >/dev/null
$PY tools/check_screen.py box "$F/run/dump.bin" "$F/run/dump.bin" >/dev/null || fail "F: the win box is not shown"
$PY - "$F/run/Veranda Solitaire Stats" <<-'EOF' || fail "F: the statistics file"
	import struct, sys
	d = open(sys.argv[1], 'rb').read()
	assert d[:4] == b'VSOL' and d[4] == 1, 'magic'
	game = d[5]
	played, won, streak, best, btime, bmoves = struct.unpack_from('<6H', d, 8 + 12 * 2)
	print('      statistics: last game %d; FreeCell played %d, won %d, streak %d, best %d, best time %d s, fewest moves %d'
	      % (game, played, won, streak, best, btime, bmoves))
	assert game == 2 and played == 1 and won == 1 and streak == 1 and best == 1 and btime > 0 and bmoves == 124
	EOF
ok "F: the statistics in the system folder (\"Veranda Solitaire Stats\"): FreeCell played 1, won 1, 124 moves"
f=$(sed -n 's|^/\* shot winanim \([0-9]*\) \*/|\1|p' "$F/script.h")
shot freecell freecell-winanim "$f" 2 1 test/freecell.plan
f=$(sed -n 's|^/\* shot fcdrag \([0-9]*\) \*/|\1|p' "$F/script.h")
shot freecell freecell-drag "$f" 2 1 test/freecell.plan
"$H/soltool" render "$H/cards" "$F/shot-fcdrag.state" "$F/shot-fcdrag.bin" || fail "F: the drawing"
r=$($PY tools/check_screen.py outline "$F/shot-freecell-drag/run/dump.bin" "$F/shot-fcdrag.bin") || { echo "$r"; fail "F: the drag's screenshot"; }
echo "      freecell-drag.png: ${r#ok    }"
ok "F: screenshots freecell-drag.png, freecell-winanim.png, freecell-win.png"
echo "      timings: $(timings "$F/run/console.txt")"
if [ "${SOL_SD:-1}" != 0 ]; then
	runsd "$F/sol.cvt" "$F/sd" 200 STATE1
	states "$F" "$F/sd/out" "F (SD card)" >/dev/null
	ok "F on an SD card image: won, the same state"
fi

#---------------------------------------------------------------- O
echo "== O: Golf, Pyramid, Yukon and Spider through the mouse"
session test/others.plan 6 3 others 150
O=$OUT/others
k=$(states "$O" "$O/run" O)
screens "$O" "$O/run" O >/dev/null
ok "O: Golf #3, Pyramid #3 (pairs dragged, kings clicked), Yukon #3, Spider (4 suits) #2 by game/choose game and deal numbers: $(grep -c 'home\|->\|deal \*/\|pair' "$O/script.h") actions; the $k snapshots, state and screen, equal the host's"
for i in 1 2 3 4; do
	$PY - "$O/run/SCREEN$i" "$OUT/others-$i.png" <<-'EOF'
	import sys
	from PIL import Image
	d = open(sys.argv[1], 'rb').read()
	im = Image.new('1', (640, 480))
	im.frombytes(bytes(255 - b for b in d), 'raw', '1')
	im.save(sys.argv[2])
	EOF
done
ok "O: the screens: others-1.png (Golf), others-2.png (Pyramid), others-3.png (Yukon), others-4.png (Spider)"

#---------------------------------------------------------------- G
echo "== G: the waste drawn again"
session test/waste.plan 0 1 waste 60
G=$OUT/waste
states "$G" "$G/run" G >/dev/null
screens "$G" "$G/run" G >/dev/null
$PY - "$G/run/SCREEN1" "$OUT/waste.png" <<-'EOF'
	import sys
	from PIL import Image
	d = open(sys.argv[1], 'rb').read()
	im = Image.new('1', (640, 480))
	im.frombytes(bytes(255 - b for b in d), 'raw', '1')
	im.save(sys.argv[2])
	EOF
ok "G: Klondike #1, a deal and the waste's top card home (the waste from two cards to one): the card under it drawn whole; the screen is the state drawn afresh (waste.png)"

#---------------------------------------------------------------- X
echo "== X: no moves left"
# Golf #4 (test/nomoves.plan): the talon dealt out by 16 clicks; after
# the 16th no card goes anywhere: the status bar says "No moves left"
# (src/engine.c's sol_has_move; the console's SOL NO MOVES), not before
session test/nomoves.plan 6 4 nomoves 60
X=$OUT/nomoves
states "$X" "$X/run" X >/dev/null
screens "$X" "$X/run" X >/dev/null
c=$X/run/console.txt
[ "$(grep -ac 'SOL NO MOVES' "$c")" = 1 ] || fail "X: SOL NO MOVES not once"
[ "$(grep -an 'SOL NO MOVES' "$c" | cut -d: -f1)" -gt "$(grep -an '^STATE1$' "$c" | cut -d: -f1)" ] \
	|| fail "X: No moves left before the talon was out"
$PY "$SDK/tools/vram2png.py" --color "$X/run/dump.bin" "$OUT/nomoves.png" >/dev/null
ok "X: Golf #4, the talon dealt out by 16 clicks: No moves left in the status bar then, not before (nomoves.png)"

#---------------------------------------------------------------- R
echo "== R: the dragged outline at the right of the screen"
# the user's report: a card dragged past x 512 was drawn in two, its
# right part at the left of the screen (blit.s took x / 8 from 9 of x's
# 10 bits). test/right-shots.plan: screenshots in the middle of drags from
# and to the right row (x 528): the outline across x 512 and all of it
# past; check_screen.py checks it is where the card was dragged
session test/right-shots.plan 0 1 rshots 60
R=$OUT/rshots
states "$R" "$R/run" R >/dev/null
screens "$R" "$R/run" R >/dev/null
for n in rfrom racross rend; do
	f=$(sed -n "s|^/\* shot $n \([0-9]*\) \*/|\1|p" "$R/script.h")
	shot rshots "right-$n" "$f" 0 1 test/right-shots.plan
	"$H/soltool" render "$H/cards" "$R/shot-$n.state" "$R/shot-$n.bin" || fail "R: the drawing"
	r=$($PY tools/check_screen.py outline "$OUT/rshots/shot-right-$n/run/dump.bin" "$R/shot-$n.bin" $(cat "$R/shot-$n.pos")) \
		|| { echo "$r"; fail "R: the outline of drag $n"; }
	ok "R: $n: ${r#ok    } (where the card was dragged: right-$n.png)"
done

#---------------------------------------------------------------- L
echo "== L: each game's first layout"
mkdir -p "$OUT/layouts"
felt=
for g in 0 1 2 3 4 5 6 7 8 9; do
	d=$((g * 7 + 1))
	printf 'game %d %d\nstate\nend 30\n' $g $d > "$OUT/layouts/plan$g"
	session "$OUT/layouts/plan$g" $g $d layouts/g$g 60
	Lg=$OUT/layouts/g$g
	states "$Lg" "$Lg/run" "L$g" >/dev/null
	screens "$Lg" "$Lg/run" "L$g" >/dev/null
	"$H/soltool" render "$H/cards" "$Lg/expect1" "$Lg/render.bin" || fail "L: the drawing"
	$PY tools/check_screen.py same "$Lg/run/dump.bin" "$Lg/render.bin" "$Lg/cmp.png" >/dev/null \
		|| { $PY tools/check_screen.py same "$Lg/run/dump.bin" "$Lg/render.bin"; fail "L: game $g's screen is not its deal"; }
	$PY "$VERANDA/tools/check_vera.py" "$Lg/run/dump.bin" >/dev/null || fail "L: game $g's display"
	r=$($PY tools/check_look.py look "$Lg/run/dump.bin" "$Lg/render.bin.map" "$H/cards") || { echo "$r"; fail "L: game $g's colours"; }
	felt="$felt $(echo "$r" | sed -n 's/.*the table \([0-9]*\) cards of felt.*/\1/p')"
	$PY "$SDK/tools/vram2png.py" --color "$Lg/run/dump.bin" "$OUT/layout-g$g.png" >/dev/null
done
ok "L: the 10 games' first layouts: the state is the host engine's, the screen its drawing pixel for pixel (layout-g0.png ... g9.png)"
ok "L: and their colours: the bar, the status bar mist, the table each card's tile entry as drawn afresh (felt cards:$felt)"

#---------------------------------------------------------------- M
echo "== M: the real mouse"
# the test build of $1 with the mouse script $1/mouse.txt, under R49
mouserun() {
	local d=$1 secs=$2
	mkdir -p "$d/run"
	cp "$d/sol.cvt" "$d/run/AUTOSTART.cvt"
	cp "$SDK/bin/VERANDA.PRG" "$SDK/bin/VERANDA.SYS" "$d/run/"
	cp "$d/mouse.txt" "$d/run/"
	(cd "$d/run" && SDL_VIDEODRIVER=dummy timeout -s INT "$secs" "$MEMU" -rom "$R49" -prg VERANDA.PRG -run \
		-warp -sound none -echo -dump RV -mousescript mouse.txt > console.txt 2>&1) || true
	grep -aq "the mouse script's exit" "$d/run/console.txt" || { tail -5 "$d/run/console.txt"; fail "$3: the run did not reach its end"; }
}
# the SOL_REALMOUSE build saves STATEn and SCREENn after every move (n:
# 1, 2 ... in decimal; the host folder keeps no case, so the letters once
# used here gave only about 50 names): each screen against the host's
# drawing of its state, every state with all its cards (tools/check_cards.py);
# the last state's name in $last, their number in $nmoves (globals: no
# subshell)
movescreens() {
	local d=$1 i n=0
	last=
	for i in $(seq 1 255); do
		[ -f "$d/STATE$i" ] || break
		"$H/soltool" render "$H/cards" "$d/STATE$i" "$d/render$i.bin" || fail "$2: the drawing of STATE$i"
		$PY tools/check_screen.py same "$d/SCREEN$i" "$d/render$i.bin" >/dev/null \
			|| { $PY tools/check_screen.py same "$d/SCREEN$i" "$d/render$i.bin" "$d/cmp$i.png"; fail "$2: the screen after move $i is not its state drawn afresh"; }
		last=$i
		n=$((n + 1))
	done
	[ -n "$last" ] || fail "$2: no state saved"
	$PY tools/check_cards.py $(seq -f "$d/STATE%g" 1 $n) >/dev/null \
		|| { $PY tools/check_cards.py $(seq -f "$d/STATE%g" 1 $n); fail "$2: a card lost or doubled"; }
	nmoves=$n
}
if ! { "$MEMU" -help 2>&1 || true; } | grep -q -- -mousescript; then
	echo "SKIP  $MEMU has no -mousescript"
elif [ "${SOL_MOUSE:-1}" = 0 ]; then
	echo "SKIP  SOL_MOUSE=0"
else
	M=$OUT/mouse
	mkdir -p "$M"
	"$H/mkscript" test/mouse.plan "$M" 2>"$M/mkscript.log" || { cat "$M/mkscript.log"; fail "M: the script"; }
	make -s -C veranda test SDK="$SDK" OUT="$TOP/$M" SOL_GAME=0 SOL_DEAL=1 TESTFLAGS=-DSOL_REALMOUSE >"$M/build.log" 2>&1 \
		|| { cat "$M/build.log"; fail "M: the build"; }
	mouserun "$M" 60 M
	[ -f "$M/run/STATE1" ] || fail "M: the drag did not move the card (no state after a move)"
	cmp -s "$M/expect1" "$M/run/STATE1" || { diff "$M/expect1" "$M/run/STATE1"; fail "M: the state after the drag differs from the host engine's"; }
	screens "$M" "$M/run" M >/dev/null
	$PY "$SDK/tools/vram2png.py" --color "$M/run/dump.bin" "$OUT/mouse.png" >/dev/null
	ok "M: under R49, a card dragged through the ROM's PS/2 mouse path: the state and the screen equal the host's (mouse.png)"

	# a whole session through the real mouse (host/mkscript's "realmouse")
	M=$OUT/mouse-long
	mkdir -p "$M"
	"$H/mkscript" test/mouse-long.plan "$M" 2>"$M/mkscript.log" || { cat "$M/mkscript.log"; fail "M: the long script"; }
	make -s -C veranda test SDK="$SDK" OUT="$TOP/$M" SOL_GAME=0 SOL_DEAL=1 TESTFLAGS=-DSOL_REALMOUSE >"$M/build.log" 2>&1 \
		|| { cat "$M/build.log"; fail "M: the long build"; }
	mouserun "$M" 200 "M (long)"
	movescreens "$M/run" "M (long)"
	cmp -s "$M/expect1" "$M/run/STATE$last" || { diff "$M/expect1" "$M/run/STATE$last"; fail "M: after the long session the state differs from the host engine's"; }
	"$H/soltool" render "$H/cards" "$M/expect1" "$M/end.bin" || fail "M: the drawing"
	$PY tools/check_screen.py same "$M/run/dump.bin" "$M/end.bin" >/dev/null || fail "M: the screen after the long session"
	$PY "$SDK/tools/vram2png.py" --color "$M/run/dump.bin" "$OUT/mouse-long.png" >/dev/null
	ok "M: Klondike #1, $(grep -c 'home\|->\|deal \*/' "$M/script.h") actions as PS/2 packets ($(grep -c 'move' "$M/mouse.txt") movements, $(grep -c 'down' "$M/mouse.txt") presses): $nmoves moves, each screen its state drawn afresh; the end state the host engine's (mouse-long.png)"

	#------------------------------------------------------------ N
	echo "== N: a release the program never sees (the R49 report)"
	# test/missed-release.mouse: a press and a little drag; the release and
	# a new press while the main loop is held (SOL_SLOWTICK), so the program
	# gets a press while it drags. Before the fix the first outline stayed
	# on the table (its frame inverted over the cards: the line through a
	# foundation in the user's photo)
	N=$OUT/missed
	mkdir -p "$N"
	cp "$OUT/mouse/script.bin" "$N/"
	make -s -C veranda test SDK="$SDK" OUT="$TOP/$N" SOL_GAME=0 SOL_DEAL=1 TESTFLAGS="-DSOL_REALMOUSE -DSOL_SLOWTICK" \
		>"$N/build.log" 2>&1 || { cat "$N/build.log"; fail "N: the build"; }
	a=$(grep -o '_sol_slow  *00[0-9A-F]*' "$N/sol.map" | head -1 | sed 's/.* 00//')
	[ -n "$a" ] || fail "N: no _sol_slow in the map"
	sed "s/SLOW/$a/" test/missed-release.mouse > "$N/mouse.txt"
	mouserun "$N" 60 N
	grep -aq "SOL DRAG MISSED RELEASE" "$N/run/console.txt" || fail "N: the press while dragging did not come (the scenario)"
	movescreens "$N/run" N
	"$H/soltool" render "$H/cards" "$N/run/STATE$last" "$N/end.bin" || fail "N: the drawing"
	r=$($PY tools/check_screen.py same "$N/run/dump.bin" "$N/end.bin" "$N/cmp.png") \
		|| { echo "$r"; fail "N: an outline was left on the table"; }
	$PY "$SDK/tools/vram2png.py" --color "$N/run/dump.bin" "$OUT/missed-release.png" >/dev/null
	ok "N: a press while dragging (the release lost): the drag given up, no outline left; the screen is the state drawn afresh (missed-release.png)"

	#------------------------------------------------------------ S
	echo "== S: the talon and the right of the table through the real mouse"
	# test/stock.plan: Klondike #1's talon clicked at its place until empty
	# (23 deals), the empty talon clicked (the waste turned over: round
	# 2), a deal again. test/right.plan: 78 moves as drags (every
	# foundation and every row, the right ones past x 512) and talon
	# clicks, the talon dealt out. After every move: the
	# screen its state drawn afresh (no outline left after a drop), all
	# 52 cards on the table (tools/check_cards.py); the end the host's
	for p in stock right; do
		M=$OUT/$p
		mkdir -p "$M"
		"$H/mkscript" test/$p.plan "$M" 2>"$M/mkscript.log" || { cat "$M/mkscript.log"; fail "S: the $p script"; }
		make -s -C veranda test SDK="$SDK" OUT="$TOP/$M" SOL_GAME=0 SOL_DEAL=1 TESTFLAGS=-DSOL_REALMOUSE >"$M/build.log" 2>&1 \
			|| { cat "$M/build.log"; fail "S: the $p build"; }
		mouserun "$M" 400 "S ($p)"
		movescreens "$M/run" "S ($p)"
		cmp -s "$M/expect1" "$M/run/STATE$last" || { diff "$M/expect1" "$M/run/STATE$last"; fail "S ($p): the end state differs from the host engine's"; }
		$PY "$SDK/tools/vram2png.py" --color "$M/run/dump.bin" "$OUT/$p.png" >/dev/null
		eval "n_$p=$nmoves"
	done
	M=$OUT/stock/run
	grep -q '^talon:$' "$M/STATE23" && [ "$(grep '^waste:' "$M/STATE23" | wc -w)" = 25 ] || fail "S: after 23 clicks the talon is not empty, the waste not 24"
	grep -q '^game 0 deal 1 round 2 ' "$M/STATE24" && [ "$(grep '^talon:' "$M/STATE24" | wc -w)" = 24 ] \
		&& [ "$(grep '^waste:' "$M/STATE24" | wc -w)" = 2 ] || fail "S: the empty talon's click did not turn the waste over"
	[ "$(grep '^talon:' "$M/STATE25" | wc -w)" = 23 ] || fail "S: no deal after the turn"
	ok "S: the talon clicked at its place 25 times: dealt to its end (23), the waste turned over (round 2, a card to the waste), dealt again; each of the $n_stock screens its state, 52 cards each (stock.png)"
	grep -q '^talon:$' "$OUT/right/run/STATE$last" || fail "S: the right session did not deal the talon out"
	ok "S: $n_right moves by drags and talon clicks onto every foundation and row (x 48-583): after each drop no outline left, the screen its state; 52 cards each; the end the host's (right.png)"

	#------------------------------------------------------------ Q
	echo "== Q: deals again and again, then quit to the shell (the R49 crash)"
	# test/deals.plan, the test build under R49: 120 new deals, restarts
	# and draw 3 / draw 1 by the menus, deal numbers typed; the end state
	# and screen the host's
	export EXTRA=
	ROM_SAVE=$ROM EMU_SAVE=$EMU
	ROM=$R49 EMU=$MEMU
	session test/deals.plan 0 1 deals 300
	ROM=$ROM_SAVE EMU=$EMU_SAVE
	D=$OUT/deals
	states "$D" "$D/run" Q >/dev/null
	screens "$D" "$D/run" Q >/dev/null
	ok "Q: $(grep -c '^menu\|^dealno' test/deals.plan) deals by the menus and deal numbers under R49: the end state and screen the host's"
	# the release build, as on the card, quits to the shell from an SD card
	# image (the shell built from build/vsrc). The kernel loads the shell
	# over the program before it resets the interrupt's vectors: with the
	# frame counter's routine still on, the interrupt ran the shell's bytes
	# where vr_set_irq's trampoline was ("BRK at $46D2 (RAM bank 0)")
	Q=$OUT/quit
	mkdir -p "$Q/card"
	rm -rf "$Q/shell"
	cp -r build/vsrc/shell "$Q/shell"
	make -s -C "$Q/shell" SDK="$SDK" >"$Q/shell.log" 2>&1 || { cat "$Q/shell.log"; fail "Q: the shell does not build"; }
	make -s -C veranda SDK="$SDK" OUT="$TOP/$Q/app" >"$Q/app.log" 2>&1 || { cat "$Q/app.log"; fail "Q: the release build"; }
	"$H/mkscript" test/quit.plan "$Q" 2>"$Q/mkscript.log" || { cat "$Q/mkscript.log"; fail "Q: the script"; }
	cp "$Q/app/sol.cvt" "$Q/card/AUTOSTART.cvt"
	cp "$Q/shell/SHELL.cvt" "$SDK/bin/VERANDA.SYS" "$Q/card/"
	$PY "$SDK/tools/vcard.py" make --mb 64 "$Q/sd.img" "$Q/card" >/dev/null || fail "Q: vcard make"
	cp "$SDK/bin/VERANDA.PRG" "$Q/"
	(cd "$Q" && SDL_VIDEODRIVER=dummy timeout -s INT 120 "$MEMU" -rom "$R49" -sdcard sd.img -prg VERANDA.PRG -run \
		-warp -sound none -echo -dump RV -mousescript mouse.txt > console.txt 2>&1) || true
	grep -aq "the mouse script's exit" "$Q/console.txt" || { tail -5 "$Q/console.txt"; fail "Q: the run did not reach its end"; }
	grep -aq "SOLITAIRE RUNNING" "$Q/console.txt" || fail "Q: Solitaire did not start"
	grep -a "PANIC" "$Q/console.txt" && fail "Q: a crash on the way to the shell"
	grep -aq "SHELL STARTED" "$Q/console.txt" || { tail -5 "$Q/console.txt"; fail "Q: the shell did not start after game/quit"; }
	$PY "$SDK/tools/vram2png.py" --color "$Q/dump.bin" "$OUT/quit.png" >/dev/null
	ok "Q: the release build quits to the shell from an SD card under R49: no crash, the shell started (quit.png)"
fi

echo "pictures: $OUT/layout-g0.png ... layout-g9.png (each game's first layout); klondike-drag.png,"
echo "          klondike-windrag.png, freecell-drag.png (drags in progress); klondike-winanim.png,"
echo "          freecell-winanim.png (the win's cards); klondike-win.png, freecell-win.png (the win box);"
echo "          klondike-end.png, klondike-about.png, klondike-stats.png, mouse.png, mouse-long.png,"
echo "          missed-release.png (a release lost while dragging), waste.png, quit.png,"
echo "          right-rfrom.png, right-racross.png, right-rend.png (drags at the right), stock.png, right.png, nomoves.png"
echo "GATE veranda-solitaire: PASS"
