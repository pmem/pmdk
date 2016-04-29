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
 * util_poolset_foreach.c -- unit test for util_poolset_foreach_part()
 *
 * usage: util_poolset_foreach file...
 */

#include "unittest.h"
#include "util.h"
#include <errno.h>

#define LOG_PREFIX "ut"
#define LOG_LEVEL_VAR "TEST_LOG_LEVEL"
#define LOG_FILE_VAR "TEST_LOG_FILE"
#define MAJOR_VERSION 1
#define MINOR_VERSION 0

/*
 * Declaration of out_init and out_fini functions because it is not
 * possible to include both unittest.h and out.h headers due to
 * redeclaration of some macros.
 */
void out_init(const char *log_prefix, const char *log_level_var,
		const char *log_file_var, int major_version,
		int minor_version);
void out_fini(void);

static int
cb(const char *name, void *arg)
{
	char *set_name = (char *)arg;
	UT_OUT("%s: %s", set_name, name);
	return 0;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "util_poolset_foreach");

	out_init(LOG_PREFIX, LOG_LEVEL_VAR, LOG_FILE_VAR,
			MAJOR_VERSION, MINOR_VERSION);
	util_init();

	if (argc < 2)
		UT_FATAL("usage: %s file...",
			argv[0]);

	for (int i = 1; i < argc; i++) {
		char *fname = argv[i];
		int ret = util_poolset_foreach_part(fname, cb, fname);

		UT_OUT("util_poolset_foreach_part(%s): %d", fname, ret);
	}
	out_fini();

	DONE(NULL);
}
