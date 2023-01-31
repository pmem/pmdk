// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2023, Intel Corporation */

/*
 * buffons_needle_problem.c <path> [<n>] -- example illustrating
 *		usage of libpmemobj
 *
 * Calculates pi number by solving Buffon's needle problem.
 * Takes one/two arguments -- path of the file and integer amount of trials
 * or only path when continuing simulation after interruption.
 * The greater number of trials, the higher calculation precision.
 */

#include <ex_common.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <time.h>
#include <libpmemobj.h>

/*
 * Layout definition
 */
POBJ_LAYOUT_BEGIN(pi);
POBJ_LAYOUT_ROOT(pi, struct my_root)
POBJ_LAYOUT_END(pi)

/*
 * Used for changing degrees into radians
 */
#define RADIAN_CALCULATE M_PI / 180.0

static PMEMobjpool *pop;

struct my_root {
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

static void
print_usage(char *argv_main[])
{
	printf("usage: %s <path> [<n>]\n",
		argv_main[0]);
}

/*
 * random_number -- randomizes number in range [0,1]
 */
static double
random_number(void)
{
	return (double)rand() / (double)RAND_MAX;
}

int
main(int argc, char *argv[])
{
	if (argc < 2 || argc > 3) {
		print_usage(argv);
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

	srand((unsigned int)time(NULL));
	TOID(struct my_root) root = POBJ_ROOT(pop, struct my_root);
	struct my_root *const rootp_rw = D_RW(root);

	if (argc == 3) {
		const char *n = argv[2];

		char *endptr;
		errno = 0;
		uint64_t ull_n = strtoull(n, &endptr, 10);

		if (*endptr != '\0' ||
				(ull_n == ULLONG_MAX && errno == ERANGE)) {
			perror("wrong n parameter\n");
			print_usage(argv);
			pmemobj_close(pop);
			return 1;
		}

		TX_BEGIN(pop) {
			TX_ADD(root);
			rootp_rw->l = 0.9;
			rootp_rw->d = 1.0;
			rootp_rw->i = 0;
			rootp_rw->p = 0;
			rootp_rw->n = ull_n;
		} TX_END
	}

	for (; rootp_rw->i < rootp_rw->n; ) {
		TX_BEGIN(pop) {
			TX_ADD(root);
			rootp_rw->angle = random_number() *
				90 * RADIAN_CALCULATE;
			rootp_rw->x = random_number() * rootp_rw->d / 2;
			rootp_rw->sin_angle_l = rootp_rw->l /
				2 * sin(rootp_rw->angle);

			if (rootp_rw->x <= rootp_rw->sin_angle_l) {
				rootp_rw->p++;
			}

			rootp_rw->pi = (2 * rootp_rw->l *
				rootp_rw->n) / (rootp_rw->p *
				rootp_rw->d);

			rootp_rw->i++;
		} TX_END
	}

	printf("%f\n", D_RO(root)->pi);

	pmemobj_close(pop);

	return 0;
}
