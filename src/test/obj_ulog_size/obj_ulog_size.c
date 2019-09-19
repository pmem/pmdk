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
 * obj_ulog_size.c -- unit tests for pmemobj_action API and
 *		redo, undo logs
 */
#include <sys/param.h>
#include <string.h>
#include <stddef.h>

#include "unittest.h"

/*
 * lane.h -- needed for LANE_REDO_EXTERNAL_SIZE
 */
#include "lane.h"

/*
 * TX_INTENT_LOG_ENTRY_OVERHEAD -- sizeof(struct ulog_entry_val)
 */
#define TX_INTENT_LOG_ENTRY_OVERHEAD (0b01ULL << 4) /* 16 bytes */

#define LAYOUT_NAME "obj_ulog_size"

#define MIN_ALLOC 64
#define MAX_ALLOC (1024 * 1024)

/*
 * REDO_OVERFLOW -- size for trigger out of memory
 *     during redo log extension
 */
#define REDO_OVERFLOW ((size_t)((LANE_REDO_EXTERNAL_SIZE\
		/ TX_INTENT_LOG_ENTRY_OVERHEAD) + 1))

/*
 * free_pool -- frees the pool from all allocated objects
 * and releases oids dynamic array
 */
static void
free_pool(PMEMoid *oids, size_t noids)
{
	for (int i = 0; i < noids; i++) {
		pmemobj_free(&oids[i]);
		UT_ASSERT(OID_IS_NULL(oids[i]));
	}

	FREE(oids);
}

/*
 * fill_pool -- fills provided pmemobj pool with as many allocations
 * as possible. Returns array of PMEMoids allocated from the
 * provided pool. The number of valid allocation stored in the
 * returned array is stored in the noids output argument.
 */
static PMEMoid *
fill_pool(PMEMobjpool *pop, size_t *noids)
{
	size_t oids_size = 2048; /* let's start with something big enough */
	PMEMoid *oids = (PMEMoid *)MALLOC(oids_size * sizeof(PMEMoid));

	*noids = 0;
	int ret;
	/* alloc as much space as possible */
	for (size_t size = MAX_ALLOC; size >= MIN_ALLOC; size /= 2) {
		ret = 0;

		while (ret == 0) {
			ret = pmemobj_alloc(pop, &oids[*noids], size,
				0, NULL, NULL);
			if (!ret)
				(*noids)++;
			if (*noids == oids_size) {
				oids_size *= 2;
				oids = (PMEMoid *)REALLOC(oids, oids_size
					* sizeof(PMEMoid));
			}
		}
	}
	return oids;
}

/*
 * do_tx_max_alloc_tx_publish_abort -- fills the pool and then tries
 *		to overfill redo log - transaction abort expected
 */
static void
do_tx_max_alloc_tx_publish_abort(PMEMobjpool *pop)
{
	UT_OUT("do_tx_max_alloc_tx_publish_abort");
	PMEMoid *allocated = NULL;
	PMEMoid reservations[REDO_OVERFLOW];
	size_t nallocated = 0;
	struct pobj_action act[REDO_OVERFLOW];

	for (int i = 0; i < REDO_OVERFLOW; i++) {
		reservations[i] = pmemobj_reserve(pop, &act[i], MIN_ALLOC, 0);
		UT_ASSERT(!OID_IS_NULL(reservations[i]));
	}

	allocated = fill_pool(pop, &nallocated);

	/* it should abort - cannot extend redo log */
	TX_BEGIN(pop) {
		pmemobj_tx_publish(act, REDO_OVERFLOW);
	} TX_ONABORT {
		UT_OUT("!Cannot publish");
	} TX_ONCOMMIT {
		UT_FATAL("Can publish");
	} TX_END

	free_pool(allocated, nallocated);
	pmemobj_cancel(pop, act, REDO_OVERFLOW);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_ulog_size");

	if (argc != 2)
		UT_FATAL("usage: %s [file]", argv[0]);

	PMEMobjpool *pop = pmemobj_create(argv[1], LAYOUT_NAME, 0,
		S_IWUSR | S_IRUSR);

	if (pop == NULL)
		UT_FATAL("!pmemobj_create");

	do_tx_max_alloc_tx_publish_abort(pop);

	pmemobj_close(pop);

	DONE(NULL);
}

#ifdef _MSC_VER
extern "C" {
	/*
	 * Since libpmemobj is linked statically,
	 * we need to invoke its ctor/dtor.
	 */
	MSVC_CONSTR(libpmemobj_init)
	MSVC_DESTR(libpmemobj_fini)
}
#endif
