/*
 * Copyright 2016, Intel Corporation
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
 * libpmempool_feature -- pmempool_feature_(enable|disable|query) test
 *
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include "libpmempool.h"
#include "pool_hdr.h"
#include "unittest.h"

#define EMPTY_FLAGS	0

/*
 * print_usage -- print usage of program
 */
static void
print_usage(const char *name)
{
	UT_OUT("usage: %s <pool_path> (e|d|q) <feature-name>", name);
	UT_OUT("feature-name: SINGLEHDR, CKSUM_2K, SHUTDOWN_STATE");
}

/*
 * str2pmempool_feature -- convert feature name to pmempool_feature enum
 */
static enum pmempool_feature
str2pmempool_feature(const char *app, const char *str)
{
	uint32_t fval = util_str2pmempool_feature(str);
	if (fval == UINT32_MAX) {
		print_usage(app);
		UT_FATAL("unknown feature: %s", str);
	}
	return (enum pmempool_feature)fval;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "libpmempool_feature");

	if (argc < 4) {
		print_usage(argv[0]);
		UT_FATAL("insufficient number of arguments: %d", argc - 1);
	}

	const char *path = argv[1];
	char cmd = argv[2][0];
	enum pmempool_feature feature = str2pmempool_feature(argv[0], argv[3]);
	int ret;

	switch (cmd) {
	case 'e':
		return pmempool_feature_enable(path, feature, EMPTY_FLAGS);
	case 'd':
		return pmempool_feature_disable(path, feature, EMPTY_FLAGS);
	case 'q':
		ret = pmempool_feature_query(path, feature, EMPTY_FLAGS);
		if (ret < 0)
			return 1;

		UT_OUT("query %s result is %d", argv[3], ret);
		return 0;
	default:
		print_usage(argv[0]);
		UT_FATAL("unknown command: %c", cmd);
	}

	DONE(NULL);
}
