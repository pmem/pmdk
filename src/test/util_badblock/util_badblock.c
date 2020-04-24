// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2020, Intel Corporation */

/*
 * util_badblock.c -- unit test for the linux bad block API
 *
 */

#include "unittest.h"
#include "util.h"
#include "out.h"
#include "set.h"
#include "badblocks.h"
#include "os_badblock.h"
#include "set_badblocks.h"
#include "fault_injection.h"
#include "file.h"

#define MIN_POOL ((size_t)(1024 * 1024 * 8)) /* 8 MiB */
#define MIN_PART ((size_t)(1024 * 1024 * 2)) /* 2 MiB */

/*
 * do_list -- (internal) list bad blocks in the file
 */
static void
do_list(const char *path)
{
	int ret;

	struct stat st;
	if (os_stat(path, &st) < 0)
		UT_FATAL("!stat %s", path);

	struct badblocks *bbs = badblocks_new();
	if (bbs == NULL)
		UT_FATAL("!badblocks_new");

	ret = badblocks_get(path, bbs);
	if (ret)
		UT_FATAL("!badblocks_get");

	if (bbs->bb_cnt == 0 || bbs->bbv == NULL) {
		UT_OUT("No bad blocks found.");
		goto exit_free;
	}

	int file_type = util_file_get_type(path);
	if (file_type < 0)
		UT_FATAL("!Cannot read type of the file");

	UT_OUT("Found %u bad block(s):", bbs->bb_cnt);

	unsigned b;
	for (b = 0; b < bbs->bb_cnt; b++) {
		UT_OUT("%zu %zu",
			/* offset is printed in 512b sectors  */
			bbs->bbv[b].offset >> 9,
			/*
			 * length is printed in:
			 * - 512b sectors in case of DAX devices,
			 * - blocks in case of regular files.
			 */
			(file_type == TYPE_DEVDAX) ?
				bbs->bbv[b].length >> 9 :
				bbs->bbv[b].length / (unsigned)st.st_blksize);
	}

exit_free:
	badblocks_delete(bbs);
}

/*
 * do_clear -- (internal) clear bad blocks in the file
 */
static void
do_clear(const char *path)
{
	if (badblocks_clear_all(path))
		UT_FATAL("!badblocks_clear_all: %s", path);
}

/*
 * do_create -- (internal) create a pool
 */
static void
do_create(const char *path)
{
	struct pool_set *set;
	struct pool_attr attr;
	unsigned nlanes = 1;

	memset(&attr, 0, sizeof(attr));

	if (util_pool_create(&set, path, 0, MIN_POOL, MIN_PART,
				&attr, &nlanes, REPLICAS_ENABLED) != 0)
		UT_FATAL("!util_pool_create: %s", path);

	util_poolset_close(set, DO_NOT_DELETE_PARTS);
}

/*
 * do_open -- (internal) open a pool
 */
static void
do_open(const char *path)
{
	struct pool_set *set;
	const struct pool_attr attr;
	unsigned nlanes = 1;

	if (util_pool_open(&set, path, MIN_PART,
				&attr, &nlanes, NULL, 0) != 0) {
		UT_FATAL("!util_pool_open: %s", path);
	}

	util_poolset_close(set, DO_NOT_DELETE_PARTS);
}

static void
do_fault_injection(const char *path)
{
	if (!core_fault_injection_enabled())
		return;

	core_inject_fault_at(PMEM_MALLOC, 1, "badblocks_recovery_file_alloc");
	char *ret = badblocks_recovery_file_alloc(path, 0, 0);
	UT_ASSERTeq(ret, NULL);
	UT_ASSERTeq(errno, ENOMEM);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "util_badblock");
	util_init();
	out_init("UTIL_BADBLOCK", "UTIL_BADBLOCK", "", 1, 0);

	if (argc < 3)
		UT_FATAL("usage: %s file op:l|c|r|o", argv[0]);

	const char *path = argv[1];

	/* go through all arguments one by one */
	for (int arg = 2; arg < argc; arg++) {
		if (argv[arg][1] != '\0')
			UT_FATAL(
				"op must be l, c, r or o (l=list, c=clear, r=create, o=open)");

		switch (argv[arg][0]) {
		case 'l':
			do_list(path);
			break;

		case 'c':
			do_clear(path);
			break;

		case 'r':
			do_create(path);
			break;

		case 'o':
			do_open(path);
			break;
		case 'f':
			do_fault_injection(path);
			break;
		default:
			UT_FATAL(
				"op must be l, c, r or o (l=list, c=clear, r=create, o=open)");
			break;
		}
	}

	out_fini();
	DONE(NULL);
}
