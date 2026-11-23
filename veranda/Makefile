# Veranda Solitaire for Veranda (see ../README.md, ../NOTICE)
#   make        Veranda Solitaire.cvt, against an installed Veranda SDK (SDK=...)
#   make test   the test build: a scripted session (SOL_SCRIPT=<script.bin of
#               host/mkscript>, record 11), SOL_GAME, SOL_DEAL
#   make run    in the emulator (the SDK's vrun)
# OUT: where the results go (default ../build/veranda)
#
# Record 0 is the program (main RAM); record 1 overlay 1 (ui.c, the dialog
# and menu routines, the deal's set-up: run in place at $A000 of its RAM
# bank, mapped for good as appBank); records 2-9 the cards (ink and map),
# record 10 their tiles (tools/mkcards.py).
SDK      ?= ../build/sdk
OPT      ?= -O
OUT      ?= ../build/veranda
export CC65_HOME ?= $(HOME)/.local/share/cc65
CC       := cl65 -t none --cpu 65C02 $(OPT) -I $(SDK)/include --asm-include-dir $(SDK)/include --asm-include-dir $(OUT) \
            -I . -I ../src -I $(OUT) -DSOL_OVERLAY
RES      := main.c draw.c blit.s ../src/engine.c ../src/games.c ../src/layout.c
OVL      := ui.c
SOL_SCRIPT ?= $(OUT)/script.bin
SOL_GAME ?= 0
SOL_DEAL ?= 1
TEST_RES := test/play.s test/sol_test.c ../src/dump.c
TEST_OVL :=
APP      := $(OUT)/Veranda Solitaire.cvt
OBJ      := $(OUT)/obj

all: $(OUT)/sol.cvt

$(OUT)/cards.h: ../tools/mkcards.py
	mkdir -p $(OUT)
	python3 -I -B ../tools/mkcards.py $(OUT)

$(OUT)/sol.bin: $(RES) $(OVL) ui.h ../src/sol.h ../src/layout.h $(OUT)/cards.h $(EXTRA_RES) $(EXTRA_OVL)
	mkdir -p $(OBJ)
	for f in $(OBJ)/*.o; do [ -f "$$f" ] && rm -f "$$f"; done; true
	for f in $(RES) $(EXTRA_RES); do $(CC) $(CFLAGS) -c -o $(OBJ)/r_$$(basename $$f).o $$f || exit 1; done
	for f in $(OVL) $(EXTRA_OVL); do $(CC) $(CFLAGS) --code-name OVERLAY1 -c -o $(OBJ)/o_$$(basename $$f).o $$f || exit 1; done
	cl65 -t none -C $(SDK)/cfg/app-c-ovl.cfg -m $(OUT)/sol.map -o $@ $(OBJ)/*.o $(SDK)/lib/veranda.lib

$(OUT)/sol.cvt: $(OUT)/sol.bin icon.pbm tile.txt app.hdr
	python3 -I -B $(SDK)/tools/vpack.py --load 0x0801 -o "$(APP)" app.hdr $(OUT)/sol.bin $(OUT)/sol.bin.1 \
		$(OUT)/cards.1 $(OUT)/cards.2 $(OUT)/cards.3 $(OUT)/cards.4 $(OUT)/cards.5 $(OUT)/cards.6 \
		$(OUT)/cards.7 $(OUT)/cards.8 $(OUT)/tiles.1 $(RECS)
	cp "$(APP)" $@

test:
	rm -f $(OUT)/sol.bin
	$(MAKE) OUT=$(OUT) CFLAGS="-DSOL_TEST -DSOL_GAME=$(SOL_GAME) -DSOL_DEAL=$(SOL_DEAL)UL $(TESTFLAGS)" \
		EXTRA_RES="$(TEST_RES)" EXTRA_OVL="$(TEST_OVL)" RECS="$(SOL_SCRIPT)" all

run: all
	$(SDK)/tools/vrun "$(APP)"

clean:
	rm -f $(OUT)/sol.bin $(OUT)/sol.bin.* $(OUT)/sol.map $(OUT)/sol.cvt "$(APP)" $(OUT)/cards.* $(OUT)/tiles.* $(OBJ)/*.o

.PHONY: all test run clean
