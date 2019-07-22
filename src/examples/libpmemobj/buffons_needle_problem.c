/*
 * Copyright 2019, Intel Corporation
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
 * buffons_needle_problem.c <path> <n> -- example illustrating
 *		usage of libpmemobj
 *
 * Calculates pi number by solving Buffon's needle problem.
 * Takes two arguments -- path of the file and integer amount of trials.
 * The greater number of trials, the higher calculation precision.
 */

#include <ex_common.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <libpmemobj.h>
/*
 * Layout definition
 */
POBJ_LAYOUT_BEGIN(pi);
POBJ_LAYOUT_ROOT(pi, struct my_root)
POBJ_LAYOUT_TOID(pi, struct pi_task)
POBJ_LAYOUT_END(pi)
/*
 * Used for changing degrees into radians
 */
#define RADIAN_CALCULATE M_PI / 180.0

static PMEMobjpool *pop;

struct pi_task {
	double x; /* coordinate of the needle's center */
	double angle; /* angle between vertical position and the needle */
	double l; /* length of the needle */
	double sin_angle_l; /* sin(angle) * l */
	double pi; /* calculated pi number */
	double d; /* distance between lines on the board */
	uint64_t i; /* variable used in for loop */
	uint64_t p; /* amount of the positive trials */
	uint64_t n; /* amount of the trials */
};

struct my_root {
	struct pi_task pi;
};

int
main(int argc, char *argv[])
{
	if (argc != 3) {
		printf("usage: %s file-name n\n", argv[0]);
		return 1;
	}

	const char *path = argv[1];

	if (file_exists(path) != 0) {
		if ((pop = pmemobj_create(path, POBJ_LAYOUT_NAME(pi),
				PMEMOBJ_MIN_POOL, 0666)) == NULL) {
			perror("failed to create pool\n");
			return 1;
		}
	} else {
		if ((pop = pmemobj_open(path,
				POBJ_LAYOUT_NAME(pi))) == NULL) {
			perror("failed to open pool\n");
			return 1;
		}
	}

	const char *n = argv[2];

	if (atoi(n) < 1) {
		printf("Wrong n parameter\n");
		return 1;
	}

	srand(time(NULL));
	TOID(struct my_root) root = POBJ_ROOT(pop, struct my_root);

	TX_BEGIN(pop) {
		D_RW(root)->pi.l = 0.9;
		D_RW(root)->pi.d = 1.0;
		D_RW(root)->pi.i = 0;
		D_RW(root)->pi.p = 0;
		D_RW(root)->pi.n = strtoull(n, NULL, 0);
	} TX_END

	for (; D_RO(root)->pi.i < D_RO(root)->pi.n; D_RW(root)->pi.i++) {
		TX_BEGIN(pop) {
			D_RW(root)->pi.angle = (double)rand() /
				(RAND_MAX) * 90 * RADIAN_CALCULATE;
			D_RW(root)->pi.x = (double)rand() /
				RAND_MAX * D_RO(root)->pi.d / 2;
			D_RW(root)->pi.sin_angle_l = D_RO(root)->pi.l /
				2 * sin(D_RO(root)->pi.angle);

			if (D_RO(root)->pi.x <= D_RO(root)->pi.sin_angle_l) {
				D_RW(root)->pi.p++;
			}

			D_RW(root)->pi.pi = (2 * D_RO(root)->pi.l *
				D_RO(root)->pi.n) / (D_RO(root)->pi.p *
				D_RO(root)->pi.d);
		} TX_END
	}

	printf("%f\n", D_RO(root)->pi.pi);

	pmemobj_close(pop);

	return 0;
}
