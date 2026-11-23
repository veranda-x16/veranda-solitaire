/*
 * Veranda Solitaire's host tool (gcc, the engine as the X16 runs it):
 *
 *   soltool deal G N              the layout of game G, deal N
 *   soltool replay G N FILE       the moves of FILE played; the state
 *                                 (FILE: m FROM TO N, c S, d S, u, r, a 0/1)
 *   soltool render CARDS STATE OUT the table of a state (dump.c's lines)
 *                                 drawn from scratch: OUT, 38400 bytes
 *                                 (80 a row, VERA's foreground), and
 *                                 OUT.map: the colour, the tile map
 *                                 entries of the 80 x 60 cards (2 bytes
 *                                 each, low first; the table rows 2-57)
 *
 * GPL-3.0-or-later (see COPYING).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "host.h"

int load_pics(const char *dir);
extern unsigned short cmap[60 * 80];
void render(u8 *fb);
int read_state(const char *path);

int main(int argc, char **argv)
{
    if (argc == 5 && !strcmp(argv[1], "render")) {
        static u8 fb[38400];
        FILE *o;
        if (!load_pics(argv[2]) || !read_state(argv[3])) {
            fprintf(stderr, "soltool: no pictures or no state\n");
            return 2;
        }
        render(fb);
        o = fopen(argv[4], "wb");
        fwrite(fb, sizeof fb, 1, o);
        fclose(o);
        {
            char p[500];
            snprintf(p, sizeof p, "%s.map", argv[4]);
            o = fopen(p, "wb");
            for (int k = 0; k < 60 * 80; ++k) {
                fputc(cmap[k] & 0xFF, o);
                fputc(cmap[k] >> 8, o);
            }
            fclose(o);
        }
        return 0;
    }
    if (argc == 4 && !strcmp(argv[1], "deal")) {
        sol_new((u8)atoi(argv[2]), (u32)strtoul(argv[3], 0, 10));
        print_state(stdout, 0);
        return 0;
    }
    if (argc == 5 && !strcmp(argv[1], "replay")) {
        FILE *f = fopen(argv[4], "r");
        char line[200];
        int n = 0, r;
        if (!f) {
            perror(argv[4]);
            return 2;
        }
        sol_new((u8)atoi(argv[2]), (u32)strtoul(argv[3], 0, 10));
        while (fgets(line, sizeof line, f)) {
            ++n;
            r = do_line(line, 0);
            if (r == 0 || r < 0) {
                fprintf(stderr, "%s:%d: %s: %s", argv[4], n, r ? "bad line" : "refused", line);
                return 1;
            }
        }
        print_state(stdout, 1);
        return 0;
    }
    fprintf(stderr, "usage: soltool deal G N | replay G N FILE\n");
    return 2;
}
