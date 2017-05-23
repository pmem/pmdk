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
 * life_common.c -- shared code for pmemcto life examples
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#include <libpmemcto.h>
#include "life.h"

/*
 * game_init -- creates/opens the pool and initializes game state
 */
struct game *
game_init(const char *path, int width, int height, int percent)
{
	/* create the pmemcto pool or open it if already exists */
	PMEMctopool *pcp = pmemcto_create(path, LAYOUT_NAME, POOL_SIZE, 0666);
	if (pcp == NULL)
		pcp = pmemcto_open(path, LAYOUT_NAME);

	if (pcp == NULL) {
		fprintf(stderr, "%s", pmemcto_errormsg());
		return NULL;
	}

	/* get the root object pointer */
	struct game *gp = pmemcto_get_root_pointer(pcp);

	/* check if width/height is the same */
	if (gp != NULL) {
		if (gp->width == width && gp->height == height) {
			return gp;
		} else {
			fprintf(stderr, "board dimensions changed");
			pmemcto_free(pcp, gp->board1);
			pmemcto_free(pcp, gp->board2);
			pmemcto_free(pcp, gp);
		}
	}

	/* allocate root object */
	gp = pmemcto_calloc(pcp, 1, sizeof(*gp));
	if (gp == NULL) {
		fprintf(stderr, "%s", pmemcto_errormsg());
		return NULL;
	}

	/* save the root object pointer */
	pmemcto_set_root_pointer(pcp, gp);

	gp->pcp = pcp;

	gp->width = width;
	gp->height = height;

	gp->board1 = (char *)pmemcto_malloc(pcp, width * height);
	if (gp->board1 == NULL) {
		fprintf(stderr, "%s", pmemcto_errormsg());
		return NULL;
	}

	gp->board2 = (char *)pmemcto_malloc(pcp, width * height);
	if (gp->board2 == NULL) {
		fprintf(stderr, "%s", pmemcto_errormsg());
		return NULL;
	}

	gp->board = gp->board2;

	srand((unsigned)time(NULL));

	for (int x = 0; x < width; x++)
		for (int y = 0; y < height; y++)
			CELL(gp, gp->board, x, y) = (rand() % 100 < percent);

	return gp;
}

/*
 * cell_next -- calculates next state of given cell
 */
static int
cell_next(struct game *gp, char *b, int x, int y)
{
	int alive = CELL(gp, b, x, y);
	int neighbors = CELL(gp, b, x - 1, y - 1) +
		CELL(gp, b, x - 1, y) +
		CELL(gp, b, x - 1, y + 1) +
		CELL(gp, b, x, y - 1) +
		CELL(gp, b, x, y + 1) +
		CELL(gp, b, x + 1, y - 1) +
		CELL(gp, b, x + 1, y) +
		CELL(gp, b, x + 1, y + 1);

	int next = (alive && (neighbors == 2 || neighbors == 3)) ||
		(!alive && (neighbors == 3));

	return next;
}

/*
 * game_next -- calculates next iteration of the game
 */
void
game_next(struct game *gp)
{
	char *prev = gp->board;
	char *next = (gp->board == gp->board2) ? gp->board1 : gp->board2;

	for (int x = 0; x < gp->width; x++)
		for (int y = 0; y < gp->height; y++)
			CELL(gp, next, x, y) = cell_next(gp, prev, x, y);

	gp->board = next;
}
