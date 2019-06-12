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
 * pmreorder_xadd_range.c -- test for store reordering using
 * pmemobj_tx_xadd_range with NO_DRAIN flag
 *
 * usage: pmreorder_xadd_range g|c file
 *
 * g - write data within a transaction
 * c - check data consistency
 *
 */

#include <string.h>
#include <stddef.h>

#include "unittest.h"
#include "valgrind_internal.h"

#define DATA_SIZE	20
#define OBJ_TYPE_NUM	2
#define TEST_VALUE	4

TOID_DECLARE(struct object, OBJ_TYPE_NUM);

struct object {
	char data[DATA_SIZE];
};

/*
 * constructor -- persistent allocation constructor which sets values
 * in data array to provided argument value
 */
static int
constructor(PMEMobjpool *pop, void *ptr, void *arg)
{
	struct object *o = (struct object *)ptr;
	for (int i = 0; i < DATA_SIZE; ++i) {
		o->data[i] = *(char *)(arg);
	}
	pmemobj_persist(pop, o->data, DATA_SIZE * sizeof(o->data[0]));
	return 0;
}

/*
 * write data -- write data to pool in transaction, using
 * POBJ_XADD_NO_DRAIN flag in order to add multiple ranges with only one fence
 * at the end
 */
static void
write_data(PMEMobjpool *pop)
{
	TOID(struct object) obj;
	int value = TEST_VALUE;
	POBJ_ALLOC(pop, &obj, struct object, sizeof(struct object),
				constructor, &value);

	char tmp[DATA_SIZE];
	memcpy(tmp, D_RO(obj)->data, DATA_SIZE);

	TX_BEGIN(pop) {

		VALGRIND_PMC_EMIT_LOG("TX_ADD.BEGIN");

		TX_XADD_FIELD(obj, data[1], POBJ_XADD_NO_DRAIN);
		D_RW(obj)->data[1] = 1;

		TX_XADD_FIELD(obj, data[3], POBJ_XADD_NO_DRAIN);
		D_RW(obj)->data[3] = 3;

		TX_XADD_FIELD(obj, data[5], POBJ_XADD_NO_DRAIN);
		D_RW(obj)->data[5] = 5;

		TX_XADD_FIELD(obj, data[6], POBJ_XADD_NO_DRAIN);
		D_RW(obj)->data[6] = 6;

		TX_ADD_FIELD(obj, data[8]);
		D_RW(obj)->data[8] = 8;

		VALGRIND_PMC_EMIT_LOG("TX_ADD.END");

		pmemobj_tx_abort(-1);

	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERTeq(memcmp(D_RO(obj)->data, tmp, DATA_SIZE), 0);
}

/*
 * check_data -- check pool data consistency
 */
static int
check_data(PMEMobjpool *pop)
{
	TOID(struct object) o = POBJ_FIRST(pop, struct object);
	for (int i = 0; i < DATA_SIZE; ++i) {
		if (D_RO(o)->data[i] != TEST_VALUE)
			return 1;
	}
	return 0;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmreorder_xadd_range");

	if (argc != 3) {
		UT_FATAL("usage: %s g|c [file]", argv[0]);
	}

	char opt = argv[1][0];

	PMEMobjpool *pop;
	if ((pop = pmemobj_open(argv[2], NULL)) == NULL) {
		UT_FATAL("!pmemobj_open");
	}

	int ret = 0;
	switch (opt) {
		case 'g':
			write_data(pop);
			break;
		case 'c':
			ret = check_data(pop);
			if (ret) {
				pmemobj_close(pop);
				UT_FATAL("Data is inconsistent");
			}
			break;
		default:
			UT_FATAL("Unrecognized option %c", opt);
	}

	pmemobj_close(pop);

	DONE(NULL);
}
