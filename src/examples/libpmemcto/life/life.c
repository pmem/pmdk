/*
 * Copyright 2017, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * life.c -- a simple example which implements Conway's Game of Life
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ncurses.h>

#include <libpmemcto.h>
#include "life.h"

#define WIDTH 64
#define HEIGHT 64

/*
 * game_draw -- displays game board
 */
static void
game_draw(WINDOW *win, struct game *gp)
{
	for (int x = 0; x < gp->width; x++) {
		for (int y = 0; y < gp->height; y++) {
			if (CELL(gp, gp->board, x, y))
				mvaddch(x + 1, y + 1, 'O');
			else
				mvaddch(x + 1, y + 1, ' ');
		}
	}
	wborder(win, '|', '|', '-', '-', '+', '+', '+', '+');
	wrefresh(win);
}

int
main(int argc, const char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "life path [iterations]\n");
		exit(1);
	}

	unsigned iterations = ~0; /* ~inifinity */
	if (argc == 3)
		iterations = atoi(argv[2]);

	struct game *gp = game_init(argv[1], WIDTH, HEIGHT, 10);
	if (gp == NULL)
		exit(1);

	initscr();
	noecho();

	WINDOW *win = newwin(HEIGHT + 2, WIDTH + 2, 0, 0);

	while (iterations > 0) {
		game_draw(win, gp);
		game_next(gp);

		timeout(500);
		if (getch() != -1)
			break;

		iterations--;
	}

	endwin();

	pmemcto_close(gp->pcp);

	return 0;
}
