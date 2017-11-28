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
 * pmem_sds.c -- unit test for shutdown status functions
 */

#include "unittest.h"
#include <stdlib.h>

/* use 4k files in this example */
#define PMEM_LEN 4096

extern char **uids;
extern size_t uids_size;
extern uint64_t *uscs;
extern size_t uscs_size;

static int fail_it;

#define FAIL(X) if (X == ++fail_it) exit(0);

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmem_sds");
	size_t mapped_len = PMEM_LEN;
	int is_pmem;

	if (argc < 2)
		UT_FATAL("usage: %s init fail (file uuid usc)...", argv[0]);

	int files = (argc - 2) / 3;

	char **pmemaddr = malloc(files * sizeof(char *));
	uids = malloc(files * sizeof(uids[0]));
	uscs = malloc(files * sizeof(uscs[0]));
	uids_size = files;
	uscs_size = files;

	int init = atoi(argv[1]);
	int fail_on = atoi(argv[2]);
	argv = argv + 3;
	for (int i = 0; i < files; i++) {
		if ((pmemaddr[i] = pmem_map_file(argv[i * 3], PMEM_LEN,
				PMEM_FILE_CREATE, 0666, &mapped_len,
					&is_pmem)) == NULL) {
			UT_FATAL("pmem_map_file");
		}

		uids[i] = argv[i * 3 + 1];
		uscs[i] = strtoul(argv[i * 3 + 2], NULL, 0);
	}
	FAIL(fail_on);

	PMEMsds *pool_sds = (PMEMsds *)pmemaddr[0];
	if (init) {
		/* initialize pool shutdown state */
		pmem_init_shutdown_state(pool_sds);
		FAIL(fail_on);
		for (int i = 0; i < files; i++) {
			pmem_add_to_shutdown_state(pool_sds, argv[2 + i]);
			FAIL(fail_on);
		}
	} else {
		/* verify a shutdown status saved in the pool */
		PMEMsds current_sds;
		pmem_init_shutdown_state(&current_sds);
		FAIL(fail_on);
		for (int i = 0; i < files; i++) {
			pmem_add_to_shutdown_state(&current_sds, argv[2 + i]);
			FAIL(fail_on);
		}

		if (pmem_check_shutdown_state(&current_sds, pool_sds)) {
			UT_FATAL(
				"An adr failure is detected, the pool might be corrupted");
		}
	}
	FAIL(fail_on);
	pmem_set_shutdown_flag(pool_sds);

	/* pool is open */
	FAIL(fail_on);

	/* close pool */
	pmem_clear_shutdown_flag(pool_sds);
	FAIL(fail_on);

	for (int i = 0; i < files; i++)
		pmem_unmap(pmemaddr[i], mapped_len);
	DONE(NULL);
}
