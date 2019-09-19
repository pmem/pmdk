/*
 * Copyright 2018-2019, Intel Corporation
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
 * util_badblock.c -- unit test for the linux bad block API
 *
 */

#include "unittest.h"
#include "util.h"
#include "out.h"
#include "set.h"
#include "os_dimm.h"
#include "os_badblock.h"
#include "badblock.h"

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

	ret = os_badblocks_get(path, bbs);
	if (ret)
		UT_FATAL("!os_badblocks_get");

	if (bbs->bb_cnt == 0 || bbs->bbv == NULL) {
		UT_OUT("No bad blocks found.");
		goto exit_free;
	}

	UT_OUT("Found %u bad block(s):", bbs->bb_cnt);

	unsigned b;
	for (b = 0; b < bbs->bb_cnt; b++) {
		UT_OUT("%llu %u",
			/* offset is printed in 512b sectors  */
			bbs->bbv[b].offset >> 9,
			/* length is printed in blocks */
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
	if (os_badblocks_clear_all(path))
		UT_FATAL("!os_badblocks_clear_all: %s", path);
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

		default:
			UT_FATAL(
				"op must be l, c, r or o (l=list, c=clear, r=create, o=open)");
			break;
		}
	}

	out_fini();
	DONE(NULL);
}
