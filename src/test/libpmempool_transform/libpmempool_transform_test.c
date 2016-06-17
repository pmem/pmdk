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
 * libpmempool_repl_sync -- Placeholder for testing libpmempool replica.
 *
 */

#include <stddef.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include "unittest.h"

/*
 * print_usage -- print usage of program
 */
static void
print_usage(char *name)
{
	UT_OUT("Usage: %s [-f <flags>]"
			"<poolset_in_path> <poolset_out_path>\n", name);
}

int
main(int argc, char *argv[])
{
	int res;
	START(argc, argv, "libpmempool_mod_test");
	int opt;

	char *poolset_file_src = NULL;
	char *poolset_file_dst = NULL;
	unsigned flags = 0;


	while ((opt = getopt(argc, argv, "stw:g:f:i:z:d:")) != -1) {
		switch (opt) {
		case 'f':
			flags = (unsigned)strtoul(optarg, NULL, 0);
			break;
		default:
			print_usage(argv[0]);
			return -1;
		}
	}

	if (optind + 1 < argc) {
		poolset_file_src = argv[optind];
		poolset_file_dst = argv[optind + 1];
	} else {
		print_usage(argv[0]);
	}
	res = pmempool_transform(poolset_file_src, poolset_file_dst,
			flags);

	UT_OUT("Result: %d\n", res);
	if (res)
		UT_OUT("%s\n", strerror(errno));

	DONE(NULL);
}
