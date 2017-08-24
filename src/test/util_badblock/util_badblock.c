/*
 * Copyright 2017, Intel Corporation
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
 * util_badblock.c -- unit test for badblock iterator
 *
 */

#include "unittest.h"
#include "badblock.h"
#include "plugin.h"
#include "util.h"

static void
test_badblock(const char *path, int nbadblocks)
{
	struct badblock_iter *iter = badblock_new(path);
	UT_ASSERTne(iter, NULL);

	int fbadblocks = 0;

	struct badblock b;
	while (iter->i_ops.next(iter, &b) == 0) {
		fbadblocks++;
		iter->i_ops.clear(iter, &b);
	}

	UT_ASSERTeq(nbadblocks, fbadblocks);

	iter->i_ops.del(iter);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "util_badblock");
	util_init();

	if (argc != 4)
		UT_FATAL("usage: %s file nbadblocks plugin_dir", argv[0]);

	char *plugin_dir = argv[3];
	int ret = plugin_init(plugin_dir);
	UT_ASSERTeq(ret, 0);

	test_badblock(argv[1], atoi(argv[2]));

	DONE(NULL);
}
