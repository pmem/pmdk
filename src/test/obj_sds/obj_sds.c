// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2023, Intel Corporation */

/*
 * util_sds.c -- unit test for shutdown status functions
 */

#include "unittest.h"
#include "shutdown_state.h"
#include <stdlib.h>
#include <libpmemobj.h>

static char **uids;
static size_t uids_size;
static size_t uid_it;
static uint64_t *uscs;
static size_t uscs_size;
static size_t usc_it;

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_sds");

	if (argc < 2)
		UT_FATAL("usage: %s init fail file (uuid usc)...", argv[0]);

	unsigned files = (unsigned)(argc - 3) / 2;

	uids = MALLOC(files * sizeof(uids[0]));
	uscs = MALLOC(files * sizeof(uscs[0]));
	uids_size = files;
	uscs_size = files;

	int init = atoi(argv[1]);
	int fail = atoi(argv[2]);
	char *path = argv[3];

	char **args = argv + 4;
	for (unsigned i = 0; i < files; i++) {
		uids[i] = args[i * 2];
		uscs[i] = strtoull(args[i * 2 + 1], NULL, 0);
	}

	PMEMobjpool *pop;
	if (init) {
		if ((pop = pmemobj_create(path, "LAYOUT", 0, 0600)) == NULL) {
			UT_FATAL("!%s: pmemobj_create", path);
		}
#if !NDCTL_ENABLED
		pmemobj_close(pop);
		pmempool_feature_enable(path, PMEMPOOL_FEAT_SHUTDOWN_STATE, 0);
		if ((pop = pmemobj_open(path, "LAYOUT")) == NULL) {
			UT_FATAL("!%s: pmemobj_open", path);
		}
#endif
	} else {
		if ((pop = pmemobj_open(path, "LAYOUT")) == NULL) {
			UT_FATAL("!%s: pmemobj_open", path);
		}
	}

	if (!fail)
		pmemobj_close(pop);

	FREE(uids);
	FREE(uscs);

	if (fail)
		exit(1);

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

#ifdef _MSC_VER
/*
 * Since libpmemobj is linked statically, we need to invoke its ctor/dtor.
 */
MSVC_CONSTR(libpmemobj_init)
MSVC_DESTR(libpmemobj_fini)
#endif
