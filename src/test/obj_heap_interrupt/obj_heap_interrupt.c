/*
 * Copyright 2016-2019, Intel Corporation
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
 * obj_heap_interrupt.c -- unit test for pool heap interruption
 */
#include "heap_layout.h"
#include "memops.h"
#include "unittest.h"

POBJ_LAYOUT_BEGIN(heap_interrupt);
POBJ_LAYOUT_END(heap_interrupt);

static int exit_on_finish = 0;
FUNC_MOCK(operation_finish, void, struct operation_context *ctx,
	unsigned flags)
	FUNC_MOCK_RUN_DEFAULT {
		if (exit_on_finish)
			exit(0);
		else
			_FUNC_REAL(operation_finish)(ctx, flags);
	}
FUNC_MOCK_END

static void
sc0_create(PMEMobjpool *pop)
{
	PMEMoid oids[3];
	TX_BEGIN(pop) {
		oids[0] = pmemobj_tx_alloc(CHUNKSIZE - 100, 0);
		oids[1] = pmemobj_tx_alloc(CHUNKSIZE - 100, 0);
		oids[2] = pmemobj_tx_alloc(CHUNKSIZE - 100, 0);
	} TX_END

	pmemobj_free(&oids[0]);

	exit_on_finish = 1;
	pmemobj_free(&oids[1]);
}

/*
 * noop_verify -- used in cases in which a successful open means that the test
 *	have passed successfully.
 */
static void
noop_verify(PMEMobjpool *pop)
{
}

typedef void (*scenario_func)(PMEMobjpool *pop);

static struct {
	scenario_func create;
	scenario_func verify;
} scenarios[] = {
	{sc0_create, noop_verify},
};

int
main(int argc, char *argv[])
{
	START(argc, argv, "heap_interrupt");

	UT_COMPILE_ERROR_ON(POBJ_LAYOUT_TYPES_NUM(heap_interrupt) != 0);

	if (argc != 4)
		UT_FATAL("usage: %s file [cmd: c/o] [scenario]", argv[0]);

	const char *path = argv[1];

	PMEMobjpool *pop = NULL;
	int exists = argv[2][0] == 'o';
	int scenario = atoi(argv[3]);

	if (!exists) {
		if ((pop = pmemobj_create(path,
			POBJ_LAYOUT_NAME(heap_interrupt),
			0, S_IWUSR | S_IRUSR)) == NULL) {
			UT_FATAL("failed to create pool\n");
		}
		scenarios[scenario].create(pop);

		/* if we get here, something is wrong with function mocking */
		UT_ASSERT(0);
	} else {
		if ((pop = pmemobj_open(path,
			POBJ_LAYOUT_NAME(heap_interrupt)))
						== NULL) {
			UT_FATAL("failed to open pool\n");
		}
		scenarios[scenario].verify(pop);
	}

	pmemobj_close(pop);

	DONE(NULL);
}

#ifdef _MSC_VER
/*
 * Since libpmemobj is linked statically, we need to invoke its ctor/dtor.
 */
MSVC_CONSTR(libpmemobj_init)
MSVC_DESTR(libpmemobj_fini)
#endif
