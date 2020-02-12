// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2019, Intel Corporation */

/*
 * basic.c -- basic example for the librpmem
 */
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <librpmem.h>

#define POOL_SIZE	(32 * 1024 * 1024)
#define DATA_OFF	4096 /* pool header size */
#define DATA_SIZE	(POOL_SIZE - DATA_OFF)
#define NLANES		64

#define SET_POOLSET_UUID	1
#define SET_UUID		2
#define SET_NEXT		3
#define SET_PREV		4
#define SET_FLAGS		5

static void
default_attr(struct rpmem_pool_attr *attr)
{
	memset(attr, 0, sizeof(*attr));
	attr->major = 1;
	strncpy(attr->signature, "EXAMPLE", RPMEM_POOL_HDR_SIG_LEN);
	memset(attr->poolset_uuid, SET_POOLSET_UUID, RPMEM_POOL_HDR_UUID_LEN);
	memset(attr->uuid, SET_UUID, RPMEM_POOL_HDR_UUID_LEN);
	memset(attr->next_uuid, SET_NEXT, RPMEM_POOL_HDR_UUID_LEN);
	memset(attr->prev_uuid, SET_PREV, RPMEM_POOL_HDR_UUID_LEN);
	memset(attr->user_flags, SET_FLAGS, RPMEM_POOL_USER_FLAGS_LEN);
}

static int
do_create(const char *target, const char *poolset, void *pool)
{
	unsigned nlanes = NLANES;
	int ret = 0;

	struct rpmem_pool_attr pool_attr;
	default_attr(&pool_attr);

	RPMEMpool *rpp = rpmem_create(target, poolset, pool, POOL_SIZE,
			&nlanes, &pool_attr);

	if (!rpp) {
		fprintf(stderr, "rpmem_create: %s\n", rpmem_errormsg());
		return 1;
	}

	if (rpmem_persist(rpp, DATA_OFF, DATA_SIZE, 0, 0)) {
		fprintf(stderr, "rpmem_persist: %s\n", rpmem_errormsg());
		ret = 1;
	}

	if (rpmem_close(rpp)) {
		fprintf(stderr, "rpmem_close: %s\n", rpmem_errormsg());
		exit(1);
	}

	return ret;
}

static int
do_open(const char *target, const char *poolset, void *pool)
{
	unsigned nlanes = NLANES;
	int ret = 0;

	struct rpmem_pool_attr def_attr;
	default_attr(&def_attr);

	struct rpmem_pool_attr pool_attr;
	RPMEMpool *rpp = rpmem_open(target, poolset, pool, POOL_SIZE, &nlanes,
			&pool_attr);
	if (!rpp) {
		fprintf(stderr, "rpmem_open: %s\n", rpmem_errormsg());
		return 1;
	}

	if (memcmp(&def_attr, &pool_attr, sizeof(def_attr))) {
		fprintf(stderr, "remote pool not consistent\n");
		ret = 1;
		goto end;
	}

	if (rpmem_persist(rpp, DATA_OFF, DATA_SIZE, 0, 0)) {
		fprintf(stderr, "rpmem_persist: %s\n", rpmem_errormsg());
		ret = 1;
	}

end:
	if (rpmem_close(rpp)) {
		fprintf(stderr, "rpmem_close: %s\n", rpmem_errormsg());
		exit(1);
	}

	return ret;
}

static int
do_remove(const char *target, const char *poolset)
{
	if (rpmem_remove(target, poolset, 0)) {
		fprintf(stderr, "removing pool failed: %s\n", rpmem_errormsg());
		return 1;
	}

	return 0;
}

enum op_t {op_create, op_open, op_remove, op_max};

static const char *str2op[] = {
	[op_create] = "create",
	[op_open] = "open",
	[op_remove] = "remove"
};

static void
parse_args(int argc, char *argv[], enum op_t *op, const char **target,
		const char **poolset)
{
	if (argc < 4) {
		fprintf(stderr, "usage:\t%s [create|open|remove] "
				"<target> <poolset>\n", argv[0]);
		exit(1);
	}

	/* convert string to op */
	*op = op_max;
	const size_t str2op_size = sizeof(str2op) / sizeof(str2op[0]);
	for (int i = 0; i < str2op_size; ++i) {
		if (strcmp(str2op[i], argv[1]) == 0) {
			*op = (enum op_t)i;
			break;
		}
	}

	if (*op == op_max) {
		fprintf(stderr, "unsupported operation -- '%s'\n", argv[1]);
		exit(1);
	}

	*target = argv[2];
	*poolset = argv[3];
}

static void *
alloc_memory()
{
	long pagesize = sysconf(_SC_PAGESIZE);
	if (pagesize < 0) {
		perror("sysconf");
		exit(1);
	}

	/* allocate a page size aligned local memory pool */
	void *mem;
	int ret = posix_memalign(&mem, pagesize, POOL_SIZE);
	if (ret) {
		fprintf(stderr, "posix_memalign: %s\n", strerror(ret));
		exit(1);
	}

	assert(mem != NULL);

	return mem;
}

int
main(int argc, char *argv[])
{
	enum op_t op;
	const char *target, *poolset;
	parse_args(argc, argv, &op, &target, &poolset);

	void *pool = alloc_memory();
	int ret = 0;

	switch (op) {
	case op_create:
		ret = do_create(target, poolset, pool);
		break;
	case op_open:
		ret = do_open(target, poolset, pool);
		break;
	case op_remove:
		ret = do_remove(target, poolset);
		break;
	default:
		assert(0);
	}

	free(pool);
	return ret;
}
