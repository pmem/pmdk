// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018, Intel Corporation */

/*
 * obj_mem.c -- simple test for pmemobj_memcpy, pmemobj_memmove and
 * pmemobj_memset that verifies nothing blows up on pmemobj side.
 * Real consistency tests are for libpmem.
 */
#include "unittest.h"

static unsigned Flags[] = {
		0,
		PMEMOBJ_F_MEM_NODRAIN,
		PMEMOBJ_F_MEM_NONTEMPORAL,
		PMEMOBJ_F_MEM_TEMPORAL,
		PMEMOBJ_F_MEM_NONTEMPORAL | PMEMOBJ_F_MEM_TEMPORAL,
		PMEMOBJ_F_MEM_NONTEMPORAL | PMEMOBJ_F_MEM_NODRAIN,
		PMEMOBJ_F_MEM_WC,
		PMEMOBJ_F_MEM_WB,
		PMEMOBJ_F_MEM_NOFLUSH,
		/* all possible flags */
		PMEMOBJ_F_MEM_NODRAIN | PMEMOBJ_F_MEM_NOFLUSH |
			PMEMOBJ_F_MEM_NONTEMPORAL | PMEMOBJ_F_MEM_TEMPORAL |
			PMEMOBJ_F_MEM_WC | PMEMOBJ_F_MEM_WB,
};

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_mem");

	if (argc != 2)
		UT_FATAL("usage: %s [directory]", argv[0]);

	PMEMobjpool *pop = pmemobj_create(argv[1], "obj_mem", 0,
			S_IWUSR | S_IRUSR);
	if (!pop)
		UT_FATAL("!pmemobj_create");

	struct root {
		char c[4096];
	};

	struct root *r = pmemobj_direct(pmemobj_root(pop, sizeof(struct root)));

	for (int i = 0; i < ARRAY_SIZE(Flags); ++i) {
		unsigned f = Flags[i];

		pmemobj_memset(pop, &r->c[0], 0x77, 2048, f);

		pmemobj_memset(pop, &r->c[2048], 0xff, 2048, f);

		pmemobj_memcpy(pop, &r->c[2048 + 7], &r->c[0], 100, f);

		pmemobj_memcpy(pop, &r->c[2048 + 1024], &r->c[0] + 17, 128, f);

		pmemobj_memmove(pop, &r->c[125], &r->c[150], 100, f);

		pmemobj_memmove(pop, &r->c[350], &r->c[325], 100, f);

		if (f & PMEMOBJ_F_MEM_NOFLUSH)
			pmemobj_persist(pop, r, sizeof(*r));
	}

	pmemobj_close(pop);

	DONE(NULL);
}
