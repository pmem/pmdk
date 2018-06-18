/*
 * Copyright 2018, Intel Corporation
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
	}

	pmemobj_close(pop);

	DONE(NULL);
}
