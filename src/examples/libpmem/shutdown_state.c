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
 * pmem_sds.c -- example for shutdown status functions
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>

#include <libpmem.h>

/* use 4k files in this example */
#define PMEM_LEN 4096

int
main(int argc, char *argv[])
{
	size_t mapped_len = PMEM_LEN;
	int is_pmem;

	if (argc < 3) {
		fprintf(stderr, "usage: %s init files...\n", argv[0]);
		exit(1);
	}
	int init = atoi(argv[1]);
	int files = (argc - 2);

	char **pmemaddr = malloc(files * sizeof(char *));

	for (int i = 0; i < files; i++) {
		if ((pmemaddr[i] = pmem_map_file(argv[i + 2], PMEM_LEN,
				PMEM_FILE_CREATE, 0666, &mapped_len,
					&is_pmem)) == NULL) {
			perror("pmem_map_file");
			exit(1);
		}
	}

	PMEMsds *pool_sds = (PMEMsds *)pmemaddr[0];

	if (init) {
		/* initialize pool shutdown state */
		pmem_init_shutdown_state(pool_sds);
		for (int i = 0; i < files; i++) {
			pmem_add_to_shutdown_state(pool_sds, argv[i + 2]);
		}
	} else {
		/* verify a shutdown status saved in the pool */
		PMEMsds current_sds;
		pmem_init_shutdown_state(&current_sds);

		for (int i = 0; i < files; i++) {
			pmem_add_to_shutdown_state(&current_sds, argv[2 + i]);
		}

		if (pmem_check_shutdown_state(&current_sds, pool_sds)) {
			fprintf(stderr,
				"An adr failure was detected, the pool might be corrupted");
		}
	}
	pmem_set_shutdown_flag(pool_sds);

	/* pool is open */
	pmem_memset_persist(pmemaddr[0] + 1000, 1, 64);

	/* XXX: deep flush */

	/* close pool */
	pmem_clear_shutdown_flag(pool_sds);

	for (int i = 0; i < files; i++)
		pmem_unmap(pmemaddr[i], mapped_len);

	return 0;
}
