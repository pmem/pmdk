// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2019, Intel Corporation */

/*
 * util_sds.c -- unit test for shutdown state functions
 */

#include <stdlib.h>
#include "unittest.h"
#include "shutdown_state.h"
#include "pmemcommon.h"
#include "set.h"

#define PMEM_LEN 4096

static char **uids;
static size_t uids_size;
static size_t uid_it;
static uint64_t *uscs;
static size_t uscs_size;
static size_t usc_it;

#define FAIL(X, Y) \
	if ((X) == (Y)) {\
		common_fini();\
		DONE(NULL);\
		exit(0);\
	}

#define LOG_PREFIX "ut"
#define LOG_LEVEL_VAR "TEST_LOG_LEVEL"
#define LOG_FILE_VAR "TEST_LOG_FILE"
#define MAJOR_VERSION 1
#define MINOR_VERSION 0

int
main(int argc, char *argv[])
{
	START(argc, argv, "util_sds");

	common_init(LOG_PREFIX, LOG_LEVEL_VAR, LOG_FILE_VAR,
		MAJOR_VERSION, MINOR_VERSION);
	size_t mapped_len = PMEM_LEN;
	int is_pmem;

	if (argc < 2)
		UT_FATAL("usage: %s init fail (file uuid usc)...", argv[0]);

	unsigned files = (unsigned)(argc - 2) / 3;

	char **pmemaddr = MALLOC(files * sizeof(char *));
	uids = MALLOC(files * sizeof(uids[0]));
	uscs = MALLOC(files * sizeof(uscs[0]));
	uids_size = files;
	uscs_size = files;

	int init = atoi(argv[1]);
	int fail_on = atoi(argv[2]);
	char **args = argv + 3;
	for (unsigned i = 0; i < files; i++) {
		if ((pmemaddr[i] = pmem_map_file(args[i * 3], PMEM_LEN,
				PMEM_FILE_CREATE, 0666, &mapped_len,
					&is_pmem)) == NULL) {
			UT_FATAL("pmem_map_file");
		}

		uids[i] = args[i * 3 + 1];
		uscs[i] = strtoull(args[i * 3 + 2], NULL, 0);
	}
	FAIL(fail_on, 1);
	struct pool_replica *rep = MALLOC(
		sizeof(*rep) + sizeof(struct pool_set_part));

	memset(rep, 0, sizeof(*rep) + sizeof(struct pool_set_part));

	struct shutdown_state *pool_sds = (struct shutdown_state *)pmemaddr[0];
	if (init) {
		/* initialize pool shutdown state */
		shutdown_state_init(pool_sds, rep);
		FAIL(fail_on, 2);
		for (unsigned i = 0; i < files; i++) {
			if (shutdown_state_add_part(pool_sds, args[2 + i], rep))
				UT_FATAL("shutdown_state_add_part");
			FAIL(fail_on, 3);
		}
	} else {
		/* verify a shutdown state saved in the pool */
		struct shutdown_state current_sds;
		shutdown_state_init(&current_sds, NULL);
		FAIL(fail_on, 2);
		for (unsigned i = 0; i < files; i++) {
			if (shutdown_state_add_part(&current_sds,
					args[2 + i], NULL))
				UT_FATAL("shutdown_state_add_part");
			FAIL(fail_on, 3);
		}

		if (shutdown_state_check(&current_sds, pool_sds, rep)) {
			UT_FATAL(
				"An ADR failure is detected, the pool might be corrupted");
		}
	}
	FAIL(fail_on, 4);
	shutdown_state_set_dirty(pool_sds, rep);

	/* pool is open */
	FAIL(fail_on, 5);

	/* close pool */
	shutdown_state_clear_dirty(pool_sds, rep);
	FAIL(fail_on, 6);

	for (unsigned i = 0; i < files; i++)
		pmem_unmap(pmemaddr[i], mapped_len);

	FREE(pmemaddr);
	FREE(uids);
	FREE(uscs);

	common_fini();
	DONE(NULL);
}

FUNC_MOCK(os_dimm_uid, int, const char *path, char *uid, size_t *len, ...)
FUNC_MOCK_RUN_DEFAULT {
	if (uid_it < uids_size) {
		if (uid != NULL) {
			strcpy(uid, uids[uid_it]);
			uid_it++;
		} else {
			*len = strlen(uids[uid_it]) + 1;
		}
	} else {
		return -1;
	}

	return 0;
}
FUNC_MOCK_END
FUNC_MOCK(os_dimm_usc, int, const char *path, uint64_t *usc, ...)
	FUNC_MOCK_RUN_DEFAULT {
	if (usc_it < uscs_size) {
		*usc = uscs[usc_it];
		usc_it++;
	} else {
		return -1;
	}

	return 0;
}
FUNC_MOCK_END
