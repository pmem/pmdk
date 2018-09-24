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
 * obj_memcheck_register.c - tests that verifies that objects are registered
 *	correctly in memcheck
 */

#include "unittest.h"

static void
test_create(const char *path)
{
	PMEMobjpool *pop = NULL;

	if ((pop = pmemobj_create(path, "register",
		PMEMOBJ_MIN_POOL, S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create: %s", path);

	PMEMoid oid = pmemobj_root(pop, 1024);

	TX_BEGIN(pop) {
		pmemobj_tx_alloc(1024, 0);
		pmemobj_tx_add_range(oid, 0, 10);
	} TX_END

	pmemobj_close(pop);
}

static void
test_open(const char *path)
{
	PMEMobjpool *pop = NULL;

	if ((pop = pmemobj_open(path, "register")) == NULL)
		UT_FATAL("!pmemobj_open: %s", path);

	PMEMoid oid = pmemobj_root(pop, 1024);

	TX_BEGIN(pop) {
		pmemobj_tx_add_range(oid, 0, 10);
	} TX_END

	pmemobj_close(pop);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_memcheck_register");

	if (argc != 3)
		UT_FATAL("usage: %s [c|o] file-name", argv[0]);

	switch (argv[1][0]) {
	case 'c':
		test_create(argv[2]);
		break;
	case 'o':
		test_open(argv[2]);
		break;
	default:
		UT_FATAL("usage: %s [c|o] file-name", argv[0]);
		break;
	}

	DONE(NULL);
}
