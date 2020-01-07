// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2019, Intel Corporation */

/*
 * manpage.c -- example from librpmem manpage
 */
#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <librpmem.h>

#define POOL_SIGNATURE	"MANPAGE"
#define POOL_SIZE	(32 * 1024 * 1024)
#define NLANES		4

#define DATA_OFF	4096
#define DATA_SIZE	(POOL_SIZE - DATA_OFF)

static void
parse_args(int argc, char *argv[], const char **target, const char **poolset)
{
	if (argc < 3) {
		fprintf(stderr, "usage:\t%s <target> <poolset>\n", argv[0]);
		exit(1);
	}

	*target = argv[1];
	*poolset = argv[2];
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
	const char *target, *poolset;
	parse_args(argc, argv, &target, &poolset);

	unsigned nlanes = NLANES;
	void *pool = alloc_memory();
	int ret;

	/* fill pool_attributes */
	struct rpmem_pool_attr pool_attr;
	memset(&pool_attr, 0, sizeof(pool_attr));
	strncpy(pool_attr.signature, POOL_SIGNATURE, RPMEM_POOL_HDR_SIG_LEN);

	/* create a remote pool */
	RPMEMpool *rpp = rpmem_create(target, poolset, pool, POOL_SIZE,
			&nlanes, &pool_attr);
	if (!rpp) {
		fprintf(stderr, "rpmem_create: %s\n", rpmem_errormsg());
		return 1;
	}

	/* store data on local pool */
	memset(pool, 0, POOL_SIZE);

	/* make local data persistent on remote node */
	ret = rpmem_persist(rpp, DATA_OFF, DATA_SIZE, 0, 0);
	if (ret) {
		fprintf(stderr, "rpmem_persist: %s\n", rpmem_errormsg());
		return 1;
	}

	/* close the remote pool */
	ret = rpmem_close(rpp);
	if (ret) {
		fprintf(stderr, "rpmem_close: %s\n", rpmem_errormsg());
		return 1;
	}

	free(pool);

	return 0;
}
