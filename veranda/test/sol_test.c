/*
 * Veranda Solitaire's test build (make test): the scripted session
 * (play.s plays host/mkscript's script.bin, record 11, through
 * mouseSource), the state written on
 * Alt + W (and, with SOL_REALMOUSE, after every move) to STATE1,
 * STATE2 ... STATE10 ... in tools/../src/dump.c's lines, so the
 * gate can compare it with the host engine's; the emulator's cycle
 * counter for the timings. GPL-3.0-or-later (see COPYING).
 */
#include <string.h>
#include "../ui.h"
#include "../../src/dump.h"
#include "cards.h"

extern void __fastcall__ sol_play(u8 bank);

/* the script: host/mkscript's script.bin, the record after the tiles */
void sol_test_start(void)
{
#ifndef SOL_REALMOUSE       /* (else the X16's mouse: x16emu -mousescript) */
    u8 b = ovl_bank(2 + PIC_RECORDS + TILE_RECORDS);
    if (b)
        sol_play(b);
    else
        LogText("SOL NO SCRIPT");
#endif
}

static char buf[600];           /* (the state: Spider's about 550) */
static char name[10] = "STATE";
static char sname[11] = "SCREEN";
static u8 nsaved;

/* "STATE" or "SCREEN" and n in decimal (1, 2 ... 255: a name each; the
   host's folder keeps no case, so letters would not do) */
static void numbered(char *p, u8 n)
{
    if (n >= 100)
        *p++ = (char)('0' + n / 100);
    if (n >= 10)
        *p++ = (char)('0' + n / 10 % 10);
    *p++ = (char)('0' + n % 10);
    *p = 0;
}

/* the state, and the screen (VERA's foreground, 38,400 bytes) */
void sol_test_key(void)
{
    u8 h, k, was = busy;
    u16 n = sol_dump(buf, 1), i;
    busy = 1;                   /* the script waits */
    numbered(name + 5, ++nsaved);
    h = FOpen(name, 'w');
    if (h >= 2) {
        FWrite(h, buf, n);
        FClose(h);
    }
    numbered(sname + 6, nsaved);
    h = FOpen(sname, 'w');
    if (h >= 2) {
        for (k = 0; k < 64; ++k) {
            *(volatile u8 *)0x9F25 = 0;             /* VERA: port 0 */
            i = (u16)k * 600;
            *(volatile u8 *)0x9F20 = (u8)i;
            *(volatile u8 *)0x9F21 = (u8)(i >> 8);
            *(volatile u8 *)0x9F22 = 0x10;          /* increment 1 */
            for (i = 0; i < 600; ++i)
                buf[i] = *(volatile u8 *)0x9F23;
            FWrite(h, buf, 600);
        }
        FClose(h);
    }
    busy = was;
    LogText(name);
}

/* SOL_SLOWTICK: the second's process holds the main loop for 12 frames
   (sol_slow 1 meanwhile, for the mouse script's "wait"), as a slow pass of
   the main loop: a release and a press in that time reach the program as
   one press while it drags (gate N) */
volatile u8 sol_slow;

void sol_test_slow(void)
{
    u8 f0 = frames;
    sol_slow = 1;
    while ((u8)(frames - f0) < 12)
        ;
    sol_slow = 0;
}

/* the emulator's cycle counter ($9FB8: a write resets it; reading $9FB8
   latches it, then $9FB9-$9FBB): "TIME <what> <cycles, hex>" */
static const char hex[] = "0123456789ABCDEF";

void clock_reset(void)
{
    *(volatile unsigned char *)0x9FB8 = 0;
}

void clock_log(const char *what)
{
    static char line[32];
    unsigned char c[4], i, n = 5;
    c[0] = *(volatile unsigned char *)0x9FB8;
    c[1] = *(volatile unsigned char *)0x9FB9;
    c[2] = *(volatile unsigned char *)0x9FBA;
    c[3] = *(volatile unsigned char *)0x9FBB;
    memcpy(line, "TIME ", 5);
    while (*what)
        line[n++] = *what++;
    line[n++] = ' ';
    for (i = 4; i-- > 0;) {
        line[n++] = hex[c[i] >> 4];
        line[n++] = hex[c[i] & 15];
    }
    line[n] = 0;
    LogText(line);
}
