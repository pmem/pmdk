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
 * obj_tx_xadd_range_direct.c -- unit test for pmemobj_tx_xadd_range_direct
 */
#include <string.h>
#include <stddef.h>
#include <limits.h>

#include "unittest.h"
#include "util.h"


#define LAYOUT_NAME "tx_add_range"

#define OBJ_SIZE 1024

#define TEST_VALUE_1 1

enum type_number {
	TYPE_OBJ,
	TYPE_OBJ_ABORT,
};

TOID_DECLARE(struct object, 0);

struct object {
	size_t value;
	char data[OBJ_SIZE - sizeof(size_t)];
};

static ut_jmp_buf_t Jmp;

static void
signal_handler(int sig)
{
	ut_siglongjmp(Jmp);
}

static uint64_t
check_int(const char *size_str)
{
	uint64_t ret;

	switch (*size_str) {
	case 'S':
		ret = SIZE_MAX;
		break;
	case 'B':
		ret = SIZE_MAX - 1;
		break;
	case 'O':
		ret = sizeof(struct object);
		break;
	default:
		ret = ATOULL(size_str);
	}
	return ret;
}

static void
do_tx_xadd_range_direct(PMEMobjpool *pop, uint64_t size, int exp_errno)
{
	TOID(struct object) obj;

	TX_BEGIN(pop) {
		TOID_ASSIGN(obj, pmemobj_tx_zalloc(sizeof(struct object),
			TYPE_OBJ));
		char *ptr = (char *)pmemobj_direct(obj.oid);
		UT_ASSERT(!TOID_IS_NULL(obj));

		pmemobj_tx_xadd_range_direct(ptr, size, POBJ_XADD_NO_FLUSH);

		D_RW(obj)->value = TEST_VALUE_1;

	} TX_ONABORT {
		UT_ASSERTeq(errno, exp_errno);
	} TX_END

	if (exp_errno == 0) {
		UT_ASSERTeq(D_RO(obj)->value, TEST_VALUE_1);
	}
}

static void
do_tx_xadd_range_abort(PMEMobjpool *pop)
{
	struct sigaction v;
	sigemptyset(&v.sa_mask);
	v.sa_flags = 0;
	v.sa_handler = signal_handler;
	SIGACTION(SIGABRT, &v, NULL);

	char *ptr;
	TOID(struct object) obj;

	TX_BEGIN(pop) {
		TOID_ASSIGN(obj, pmemobj_tx_zalloc(sizeof(struct object),
			TYPE_OBJ));
		ptr = (char *)pmemobj_direct(obj.oid);
		UT_ASSERT(!TOID_IS_NULL(obj));
	} TX_FINALLY {

		if (!ut_sigsetjmp(Jmp)) {
			pmemobj_tx_xadd_range_direct(ptr,
				sizeof(struct object), POBJ_XADD_NO_FLUSH);
		}
	} TX_ONABORT {
		UT_ASSERTeq(errno, EINVAL);
	} TX_END

}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_tx_xadd_range_direct");

	char *path;
	uint64_t size;
	int exp_errno;

	if (argc < 3)
		UT_FATAL("usage: %s path size exp_errno ...",
			argv[0]);

	PMEMobjpool *pop;
	pop = pmemobj_create(argv[1], LAYOUT_NAME, PMEMOBJ_MIN_POOL, 0644);
	if (pop == NULL)
		UT_FATAL("!pmemobj_create");

	for (int i = 1; i + 2 < argc; i += 3) {
		path = argv[i];
		size = check_int(argv[i + 1]);
		exp_errno = ATOI(argv[i + 2]);

		UT_OUT("%s %lu %d", path, size, exp_errno);
		do_tx_xadd_range_direct(pop, size, exp_errno);
	}

	do_tx_xadd_range_abort(pop);

	pmemobj_close(pop);

	DONE(NULL);
}
