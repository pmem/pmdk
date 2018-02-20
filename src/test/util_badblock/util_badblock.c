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
 * util_badblock.c -- unit test for the linux bad block API
 *
 */

#include "unittest.h"
#include "util.h"
#include "out.h"
#include "os_dimm.h"
#include "os_badblock.h"

/*
 * do_list -- (internal) list bad blocks in the file
 */
static int
do_list(const char *path)
{
	int ret;

	struct badblocks *bbs = Zalloc(sizeof(struct badblocks));
	if (bbs == NULL)
		return -1;

	ret = os_badblocks_get(path, bbs);
	if (ret) {
		UT_OUT("Checking bad blocks failed.");
		goto exit_free;
	}

	if (bbs->bb_cnt == 0 || bbs->bbv == NULL) {
		UT_OUT("No bad blocks found.");
		goto exit_free;
	}

	UT_OUT("Found %u bad block(s):", bbs->bb_cnt);

	unsigned b;
	for (b = 0; b < bbs->bb_cnt; b++) {
		UT_OUT("%llu %u",
			bbs->bbv[b].offset >> 9,
			bbs->bbv[b].length >> 9);
	}

exit_free:
	Free(bbs->bbv);
	Free(bbs);

	return 0;
}

/*
 * do_clear -- (internal) clear bad blocks in the file
 */
static int
do_clear(const char *path)
{
	return os_badblocks_clear(path);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "util_badblock");
	util_init();
	out_init("UTIL_BADBLOCK", "UTIL_BADBLOCK", "", 1, 0);

	if (argc < 3)
		UT_FATAL("usage: %s file op:l|c", argv[0]);

	const char *path = argv[1];

	/* go through all arguments one by one */
	for (int arg = 2; arg < argc; arg++) {
		if (argv[arg][1] != '\0')
			UT_FATAL("op must be l or c (l=list, c=clear)");

		switch (argv[arg][0]) {
		case 'l':
			do_list(path);
			break;

		case 'c':
			do_clear(path);
			break;

		default:
			UT_FATAL("op must be l or c (l=list, c=clear)");
			break;
		}
	}

	out_fini();
	DONE(NULL);
}
