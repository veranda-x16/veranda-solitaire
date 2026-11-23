/* Veranda Solitaire's host tools (platform.c). GPL-3.0-or-later. */
#ifndef HOST_H
#define HOST_H
#include <stdio.h>
#include "../src/sol.h"
#include "../src/layout.h"
#include "../src/dump.h"

struct act { char op; u8 a, b, n; };

extern u8 changed_from[MAXSTACKS];
int stack_id(const char *n);
const char *stack_name(u8 s);
int do_line(const char *line, struct act *out);
void print_state(FILE *f, int header);
#endif
