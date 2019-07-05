/*
 * Copyright 2018-2019, Intel Corporation
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
 * pmreorder_stack.c -- unit test for engines pmreorder stack
 *
 * usage: pmreorder_stack w|c file
 * w - write data in a possibly inconsistent manner
 * c - check data consistency
 *
 */

#include "unittest.h"
#include "valgrind_internal.h"

/*
 * Consistent only if field 'e' is set and field 'f' is not.
 */
struct fields {
	int a;
	int b;
	int c;
	int d;

	int e;
	int f;
	int g;
	int h;

	int i;
	int j;
	int k;
	int l;
};

/*
 * write_fields -- (internal) write data in a consistent manner.
 */
static void
write_fields(struct fields *fieldsp)
{
	VALGRIND_PMC_EMIT_LOG("FIELDS_PACK_TWO.BEGIN");

	VALGRIND_PMC_EMIT_LOG("FIELDS_PACK_ONE.BEGIN");

	fieldsp->a = 1;
	fieldsp->b = 1;
	fieldsp->c = 1;
	fieldsp->d = 1;
	pmem_persist(&fieldsp->a, sizeof(int) * 4);

	VALGRIND_PMC_EMIT_LOG("FIELDS_PACK_ONE.END");

	fieldsp->e = 1;
	fieldsp->f = 1;
	fieldsp->g = 1;
	fieldsp->h = 1;
	pmem_persist(&fieldsp->e, sizeof(int) * 4);

	VALGRIND_PMC_EMIT_LOG("FIELDS_PACK_TWO.END");

	fieldsp->i = 1;
	fieldsp->j = 1;
	fieldsp->k = 1;
	fieldsp->l = 1;
	pmem_persist(&fieldsp->i, sizeof(int) * 4);
}

/*
 * check_consistency -- (internal) check struct fields consistency.
 */
static int
check_consistency(struct fields *fieldsp)
{
	int consistency = 1;
	if (fieldsp->e == 1 && fieldsp->f == 0)
		consistency = 0;

	return consistency;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmreorder_stack");

	VALGRIND_PMC_EMIT_LOG("NOT_DEFINED_BY_USER.END");

	if ((argc != 3) || (strchr("wc", argv[1][0]) == NULL) ||
			argv[1][1] != '\0')
		UT_FATAL("usage: %s w|c file", argv[0]);

	int fd = OPEN(argv[2], O_RDWR);
	size_t size;

	/* mmap and register in valgrind pmemcheck */
	void *map = pmem_map_file(argv[2], 0, 0, 0, &size, NULL);
	UT_ASSERTne(map, NULL);
	UT_ASSERT(size >= sizeof(struct fields));

	struct fields *fieldsp = map;

	char opt = argv[1][0];

	/* clear the struct to get a consistent start state for writing */
	if (strchr("w", opt))
		pmem_memset_persist(fieldsp, 0, sizeof(*fieldsp));

	switch (opt) {
		case 'w':
			write_fields(fieldsp);
			break;
		case 'c':
			return check_consistency(fieldsp);
		default:
			UT_FATAL("Unrecognized option %c", opt);
	}

	CLOSE(fd);

	DONE(NULL);
}
