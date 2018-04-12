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
 * obj_verify.c -- tool for creating and verifying a pmemobj pool
 */

#include <stddef.h>
#include <fcntl.h>

#include "libpmemobj.h"
#include "set.h"

#define SIGNATURE_LEN 10
#define NUMBER_LEN 10
#define MAX_NUMBER 999999999
#define FILL_SIZE 233 /* so that size of one record was 1024 bytes */
#define SKIP_OFFSET offsetof(struct data_s, checksum)

static const char *Signature = "OBJ_VERIFY";

POBJ_LAYOUT_BEGIN(obj_verify);
POBJ_LAYOUT_ROOT(obj_verify, struct root_s);
POBJ_LAYOUT_TOID(obj_verify, struct list_el);
POBJ_LAYOUT_END(obj_verify);

struct data_s {
	char signature[SIGNATURE_LEN];
	char number[NUMBER_LEN];
	uint32_t fill[FILL_SIZE];
	uint64_t checksum;
};

struct list_el {
	struct data_s data;
	POBJ_LIST_ENTRY(struct list_el) entries;
};

struct root_s {
	POBJ_LIST_HEAD(verify, struct list_el) list;
};

/*
 * list_element_constr -- constructor of a list element
 */
static int
list_element_constr(PMEMobjpool *pop, void *ptr, void *arg)
{
	struct list_el *el = (struct list_el *)ptr;
	long long unsigned *count = arg;

	while (*count > MAX_NUMBER)
		*count -= MAX_NUMBER;

	memcpy(el->data.signature, Signature, sizeof(el->data.signature));
	snprintf(el->data.number, NUMBER_LEN, "%09llu", *count);

	for (int i = 0; i < FILL_SIZE; i++)
		el->data.fill[i] = (uint32_t)rand();

	util_checksum(&el->data, sizeof(el->data), &el->data.checksum,
			1 /* insert */, SKIP_OFFSET);

	pmemobj_persist(pop, &el->data, sizeof(el->data));

	return 0;
}

/*
 * do_create -- (internal) create a pool to be verified
 */
static void
do_create(const char *path, const char *layout)
{
	PMEMobjpool *pop;

	if ((pop = pmemobj_create(path, layout, 0, S_IWUSR | S_IRUSR)) == NULL)
		if ((pop = pmemobj_open(path, layout)) == NULL) {
			out("!pmemobj_open: %s", path);
			exit(-1);
		}

	TOID(struct root_s) root = POBJ_ROOT(pop, struct root_s);

	out("create(%s): allocating records in the pool ...", path);

	srand((unsigned int)time(NULL));

	long long unsigned count = 0;
	while (!OID_IS_NULL(POBJ_LIST_INSERT_NEW_HEAD(pop, &D_RW(root)->list,
							entries,
							sizeof(struct list_el),
							list_element_constr,
							&count))) {
		count++;
	}

	TOID(struct list_el) first = POBJ_LIST_FIRST(&D_RO(root)->list);
	TOID(struct list_el) second = POBJ_LIST_NEXT(first, entries);

	long size = D_RO(first)->data.signature - D_RO(second)->data.signature;

	if (count) {
		out("create(%s): allocated %llu records (of size %zu)",
			path, count, size);
	} else {
		out("create(%s): pool is full", path);
	}

	pmemobj_close(pop);
}


/*
 * do_verify -- (internal) verify a poolset
 */
static void
do_verify(const char *path, const char *layout, int debug)
{
	PMEMobjpool *pop;
	long long unsigned count = 0;
	int error = 0;

	if ((pop = pmemobj_open(path, layout)) == NULL) {
		out("!pmemobj_open: %s", path);
		exit(-1);
	}

	TOID(struct root_s) root = POBJ_ROOT(pop, struct root_s);
	TOID(struct list_el) el;

	POBJ_LIST_FOREACH_REVERSE(el, &D_RO(root)->list, entries) {
		if (!util_checksum(&D_RW(el)->data, sizeof(D_RW(el)->data),
					&D_RW(el)->data.checksum,
					0 /* verify */, SKIP_OFFSET)) {
			out("verify(%s): incorrect record: %s (#%llu)",
				path, D_RW(el)->data.signature, count);
			error = 1;
			break;
		}
		if (debug) {
			out("verify(%llu): %s",
				count, D_RO(el)->data.signature);
		}
		count++;
	}

	TOID(struct list_el) first = POBJ_LIST_FIRST(&D_RO(root)->list);
	TOID(struct list_el) second = POBJ_LIST_NEXT(first, entries);

	long size = D_RO(first)->data.signature - D_RO(second)->data.signature;

	pmemobj_close(pop);

	if (error) {
		out("verify(%s): pool file contains error", path);
		exit(-1);
	}

	out(
		"verify(%s): pool file successfully verified (%llu records of size %zu)",
		path, count, size);
}

int
main(int argc, char *argv[])
{
	util_init();
	out_init("OBJ_VERIFY", "OBJ_VERIFY", "", 1, 0);

	if (argc < 4) {
		out("Usage: %s <obj_pool> <layout> <op:c|v|d>\n"
		    "Options:\n"
		    "   c - create\n"
		    "   v - verify\n"
		    "   d - verify with debug",
		    argv[0]);
		exit(-1);
	}

	const char *path = argv[1];
	const char *layout = argv[2];
	const char *op = argv[3];

	if (op[1] != '\0') {
		out(
			"op must be c, v or d (c=create, v=verify d='verify with debug')");
		exit(-1);
	}

	switch (op[0]) {
	case 'c': /* create and verify (no debug) */
		do_create(path, layout);
		do_verify(path, layout, 0);
		break;

	case 'v': /* verify (no debug) */
		do_verify(path, layout, 0);
		break;

	case 'd': /* verify with debug */
		do_verify(path, layout, 1);
		break;

	default:
		out(
			"op must be c, v or d (c=create, v=verify d='verify with debug')");
		exit(-1);
		break;
	}

	out_fini();

	return 0;
}
