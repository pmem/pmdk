// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019, Intel Corporation */

/*
 * obj_alloc.c -- unit test for pmemobj_alloc and pmemobj_zalloc
 */

#include "unittest.h"
#include "heap.h"
#include <limits.h>

POBJ_LAYOUT_BEGIN(alloc);
POBJ_LAYOUT_ROOT(alloc, struct root);
POBJ_LAYOUT_TOID(alloc, struct object);
POBJ_LAYOUT_END(alloc);

struct object {
	size_t value;
	char data[];
};

struct root {
	TOID(struct object) obj;
	char data[CHUNKSIZE - sizeof(TOID(struct object))];
};

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

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_alloc");

	const char *path;
	size_t size;
	uint64_t type_num;
	int is_oid_null;
	uint64_t flags;
	int expected_return_code;
	int expected_errno;
	int ret;

	if (argc < 8)
		UT_FATAL("usage: %s path size type_num is_oid_null flags "
			"expected_return_code expected_errno ...", argv[0]);

	PMEMobjpool *pop = NULL;
	PMEMoid *oidp;

	path = argv[1];

	pop = pmemobj_create(path, POBJ_LAYOUT_NAME(basic),
		0, S_IWUSR | S_IRUSR);
	if (pop == NULL) {
		UT_FATAL("!pmemobj_create: %s", path);
	}

	for (int i = 1; i + 6 < argc; i += 7) {
		size = (size_t)check_int(argv[i + 1]);
		type_num = check_int(argv[i + 2]);
		is_oid_null = ATOI(argv[i + 3]);
		flags = ATOULL(argv[i + 4]);
		expected_return_code = ATOI(argv[i + 5]);
		expected_errno = ATOI(argv[i + 6]);

		UT_OUT("%s %zu %lu %d %lu %d %d", path, size, type_num,
			is_oid_null, flags, expected_return_code,
			expected_errno);

		TOID(struct root) root = POBJ_ROOT(pop, struct root);

		oidp = &D_RW(root)->obj.oid;
		if (is_oid_null) {
			TOID_ASSIGN(root, OID_NULL);
			oidp = &root.oid;
		}

		ret = pmemobj_xalloc(
			pop, oidp, size, type_num, flags, NULL, NULL);

		UT_ASSERTeq(ret, expected_return_code);
		if (expected_errno != 0) {
			UT_ASSERTeq(errno, expected_errno);
		}

		if (ret == 0) {
			UT_OUT("alloc: %zu, size: %zu", size,
				pmemobj_alloc_usable_size(D_RW(root)->obj.oid));
			if (is_oid_null == 0) {
				UT_ASSERT(!TOID_IS_NULL(D_RW(root)->obj));
				UT_ASSERT(pmemobj_alloc_usable_size(
				    D_RW(root)->obj.oid) >= size);
			}
		}

		pmemobj_free(&D_RW(root)->obj.oid);
		UT_ASSERT(TOID_IS_NULL(D_RO(root)->obj));
		UT_OUT("free");

	}
	pmemobj_close(pop);
	DONE(NULL);
}
