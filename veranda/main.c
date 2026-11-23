/*
 * Veranda Solitaire: the resident part of the program (main RAM): the
 * start, the moves' log in RAM banks (it maps them, so it cannot run from
 * a bank itself), the frame counter (in the interrupt). The rest of the
 * program is ui.c (overlay 1, mapped for good as appBank), draw.c,
 * blit.s and the engine (../src).
 *
 * Copyright (C) 2026 the Veranda Solitaire authors. GPL-3.0-or-later
 * (see COPYING).
 */
#include "ui.h"

u8 busy;
volatile u8 frames;

/*---------------------------------------------------------------
  the moves' log: 4 bytes an atom, 2048 atoms a RAM bank, banks taken
  as the log grows
 ---------------------------------------------------------------*/
#define LOG_BANKS 48
static u8 lbank[LOG_BANKS], nlb;
u16 log_top, log_cur;

static u8 *atom_at(u16 i)
{
    RAM_BANK = lbank[i >> 11];
    return (u8 *)(0xA000 + ((i & 2047) << 2));
}

u8 log_put(u8 op, u8 a, u8 b, u8 n)
{
    u8 old = RAM_BANK, *p, k = (u8)(log_top >> 11);
    if (k >= nlb) {
        if (k >= LOG_BANKS || !(lbank[k] = BankAlloc(1))) {
            /* memory is full: the history starts again */
            log_top = log_cur = 0;
            k = 0;
        } else
            ++nlb;
    }
    p = atom_at(log_top);
    p[0] = op;
    p[1] = a;
    p[2] = b;
    p[3] = n;
    RAM_BANK = old;
    ++log_top;
    return 1;
}

void log_get(u16 i, u8 *a)
{
    u8 old = RAM_BANK, *p = atom_at(i);
    a[0] = p[0];
    a[1] = p[1];
    a[2] = p[2];
    a[3] = p[3];
    RAM_BANK = old;
}

void log_mark_end(u16 i)
{
    u8 old = RAM_BANK;
    *atom_at(i) |= A_END;
    RAM_BANK = old;
}

/* every frame, in the interrupt */
static void each_frame(void)
{
    ++frames;
}

/* back to the desktop. The frame counter's interrupt routine goes first:
   the kernel loads the next program over this one before it resets the
   interrupt's vectors (LdApplic, then AppReset), so the interrupt would
   run the shell's bytes where the trampoline was (on the user's R49, the
   SD card slow enough: "BRK at $46D2 (RAM bank 0)", the shell's text at
   $46BF with a STZ $00 in it) */
void leave(void)
{
    vr_set_irq(0);
    EnterDeskTop();
}

/* the RAM bank that holds overlay record n (LoadOverlay returns the error) */
u8 ovl_bank(u8 n)
{
    return LoadOverlay(n, 0) == 0 && r0 == 0xA000 ? r1L : 0;
}

int main(void)
{
    u8 b = ovl_bank(1);
    if (!b) {
        LogText("SOL NO OVERLAY");
        leave();
    }
    appBank = b;
    RAM_BANK = b;
    vr_set_irq(each_frame);
    ui_main();
    return 0;
}
