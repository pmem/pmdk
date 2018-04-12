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
#define FILL_SIZE 245 /* so that size of one record is 1024 bytes */
#define SKIP_OFFSET offsetof(struct data_s, checksum)

static const char *Signature = "OBJ_VERIFY";

POBJ_LAYOUT_BEGIN(obj_verify);
POBJ_LAYOUT_ROOT(obj_verify, struct root_s);
POBJ_LAYOUT_ROOT(obj_verify, struct data_s);
POBJ_LAYOUT_END(obj_verify);

struct data_s {
	char signature[SIGNATURE_LEN];
	char number_str[NUMBER_LEN];
	uint64_t number;
	uint32_t fill[FILL_SIZE];
	uint64_t checksum;
};

struct root_s {
	uint64_t count;
};

/*
 * record_constructor -- constructor of a list element
 */
static int
record_constructor(PMEMobjpool *pop, void *ptr, void *arg)
{
	struct data_s *rec = (struct data_s *)ptr;
	uint64_t *count = arg;

	memcpy(rec->signature, Signature, sizeof(rec->signature));
	snprintf(rec->number_str, NUMBER_LEN, "%09lu", *count);
	rec->number = *count;

	for (int i = 0; i < FILL_SIZE; i++)
		rec->fill[i] = (uint32_t)rand();

	util_checksum(rec, sizeof(*rec), &rec->checksum,
			1 /* insert */, SKIP_OFFSET);

	pmemobj_persist(pop, rec, sizeof(*rec));

	(*count)++;

	pmemobj_persist(pop, count, sizeof(*count));

	return 0;
}

/*
 * do_create -- (internal) create a pool to be verified
 */
static void
do_create(const char *path, const char *layout)
{
	struct pobj_alloc_class_desc class;
	PMEMobjpool *pop;
	PMEMoid oid;
	uint64_t count;

	srand((unsigned int)time(NULL));

	if ((pop = pmemobj_create(path, layout, 0, S_IWUSR | S_IRUSR)) == NULL)
		if ((pop = pmemobj_open(path, layout)) == NULL) {
			out("!pmemobj_open: %s", path);
			exit(-1);
		}

	TOID(struct root_s) root = POBJ_ROOT(pop, struct root_s);

	class.header_type = POBJ_HEADER_NONE;
	class.unit_size = sizeof(struct data_s);
	class.alignment = 0;
	class.units_per_block = 1000;

	if (pmemobj_ctl_set(pop, "heap.alloc_class.new.desc", &class) != 0) {
		pmemobj_close(pop);
		out("!pmemobj_ctl_set: %s", path);
		exit(-1);
	}

	out("create(%s): allocating records in the pool ...", path);

	count = D_RO(root)->count;
	while (pmemobj_xalloc(pop, &oid, class.unit_size, 0,
				POBJ_CLASS_ID(class.class_id),
				record_constructor, &D_RW(root)->count) == 0)
		;

	count = D_RO(root)->count - count;
	if (count) {
		out("create(%s): allocated %lu records (of size %zu)",
			path, count, sizeof(struct data_s));
	} else {
		out("create(%s): pool is full", path);
	}

	pmemobj_close(pop);
}


/*
 * do_verify -- (internal) verify a poolset
 */
static void
do_verify(const char *path, const char *layout)
{
	PMEMobjpool *pop;
	PMEMoid oid;
	uint64_t count = 0;
	int error = 0;

	if ((pop = pmemobj_open(path, layout)) == NULL) {
		out("!pmemobj_open: %s", path);
		exit(-1);
	}

	TOID(struct root_s) root = POBJ_ROOT(pop, struct root_s);
	TOID(struct data_s) rec;

	POBJ_FOREACH(pop, oid) {
		TOID_ASSIGN(rec, oid);
		if (!util_checksum(D_RW(rec), sizeof(*D_RW(rec)),
					&D_RW(rec)->checksum,
					0 /* verify */, SKIP_OFFSET)) {
			out("verify(%s): incorrect record: %s (#%lu)",
				path, D_RW(rec)->signature, count);
			error = 1;
			break;
		}

		count++;
	}

	if (D_RO(root)->count != count) {
		out(
			"verify(%s): incorrect number of records (is: %lu, should be: %lu)",
			path, count, D_RO(root)->count);
		error = 1;
	}

	pmemobj_close(pop);

	if (error) {
		out("verify(%s): pool file contains error", path);
		exit(-1);
	}

	out(
		"verify(%s): pool file successfully verified (%lu records of size %zu)",
		path, count, sizeof(struct data_s));
}

int
main(int argc, char *argv[])
{
	util_init();
	out_init("OBJ_VERIFY", "OBJ_VERIFY", "", 1, 0);

	if (argc < 4) {
		out("Usage: %s <obj_pool> <layout> <op:c|v>\n"
		    "Options:\n"
		    "   c - create\n"
		    "   v - verify\n",
		    argv[0]);
		exit(-1);
	}

	const char *path = argv[1];
	const char *layout = argv[2];
	const char *op;

	/* go through all arguments one by one */
	for (int arg = 3; arg < argc; arg++) {
		op = argv[arg];

		if (op[1] != '\0') {
			out("op must be c or v (c=create, v=verify)");
			exit(-1);
		}

		switch (op[0]) {
		case 'c': /* create and verify (no debug) */
			do_create(path, layout);
			break;

		case 'v': /* verify (no debug) */
			do_verify(path, layout);
			break;

		default:
			out("op must be c or v (c=create, v=verify)");
			exit(-1);
			break;
		}
	}

	out_fini();

	return 0;
}
