// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2020, Intel Corporation */

/*
 * util_sds.c -- unit test for shutdown state functions
 */

#include <stdlib.h>
#include "unittest.h"
#include "ut_pmem2.h"
#include "shutdown_state.h"
#include "set.h"

#define PMEM_LEN 4096

static char **uids;
static size_t uids_size;
static size_t uid_it;
static uint64_t *uscs;
static size_t uscs_size;
static size_t usc_it;
static pmem2_persist_fn persist;

#define FAIL(X, Y)				\
	if ((X) == (Y)) {			\
		goto out;			\
	}

int
main(int argc, char *argv[])
{
	START(argc, argv, "util_sds");

	if (argc < 2)
		UT_FATAL("usage: %s init fail (file uuid usc)...", argv[0]);

	unsigned files = (unsigned)(argc - 2) / 3;

	char **pmemaddr = MALLOC(files * sizeof(char *));
	int *fds = MALLOC(files * sizeof(fds[0]));
	struct pmem2_map **maps = MALLOC(files * sizeof(maps[0]));

	uids = MALLOC(files * sizeof(uids[0]));
	uscs = MALLOC(files * sizeof(uscs[0]));
	uids_size = files;
	uscs_size = files;

	int init = atoi(argv[1]);
	int fail_on = atoi(argv[2]);
	char **args = argv + 3;
	struct pmem2_config *cfg;
	PMEM2_CONFIG_NEW(&cfg);
	pmem2_config_set_required_store_granularity(cfg,
		PMEM2_GRANULARITY_PAGE);
	for (unsigned i = 0; i < files; i++) {
		fds[i] = OPEN(args[i * 3], O_CREAT | O_RDWR, 0666);
		POSIX_FALLOCATE(fds[i], 0, PMEM_LEN);
		struct pmem2_source *src;
		PMEM2_SOURCE_FROM_FD(&src, fds[i]);

		if (pmem2_map(&maps[i], cfg, src)) {
			UT_FATAL("pmem2_map: %s", pmem2_errormsg());
		}

		pmemaddr[0] = pmem2_map_get_address(maps[i]);

		uids[i] = args[i * 3 + 1];
		uscs[i] = strtoull(args[i * 3 + 2], NULL, 0);
		PMEM2_SOURCE_DELETE(&src);
	}

	persist = pmem2_get_persist_fn(maps[0]);
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
			if (shutdown_state_add_part(pool_sds, fds[i], rep))
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
					fds[i], NULL))
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

out:	for (unsigned i = 0; i < files; i++) {
		pmem2_unmap(&maps[i]);
		CLOSE(fds[i]);
	}

	PMEM2_CONFIG_DELETE(&cfg);
	FREE(pmemaddr);
	FREE(uids);
	FREE(uscs);
	FREE(fds);
	FREE(maps);

	DONE(NULL);
}

FUNC_MOCK(pmem2_source_device_id, int, const struct pmem2_source *src,
	char *uid, size_t *len)
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

FUNC_MOCK(pmem2_source_device_usc, int, const struct pmem2_source *src,
	uint64_t *usc)
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

int os_part_deep_common(struct pool_replica *rep, unsigned partidx, void *addr,
	size_t len, int flush);
/*
 * os_part_deep_common -- XXX temporally workaround until we will have pmem2
 *		integrated with common
 */
int
os_part_deep_common(struct pool_replica *rep, unsigned partidx, void *addr,
	size_t len, int flush)
{
	/*
	 * this is test - we don't need to deep persist anything -
	 * just call regular persist to make valgrind happy
	 */
	persist(addr, len);
	return 0;
}

#ifdef _MSC_VER
MSVC_CONSTR(libpmem2_init)
MSVC_DESTR(libpmem2_fini)
#endif
