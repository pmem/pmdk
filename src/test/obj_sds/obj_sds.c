/*
 * Copyright 2017-2019, Intel Corporation
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
#if !defined(_WIN32) && !NDCTL_ENABLED
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

#ifdef _MSC_VER
/*
 * Since libpmemobj is linked statically, we need to invoke its ctor/dtor.
 */
MSVC_CONSTR(libpmemobj_init)
MSVC_DESTR(libpmemobj_fini)
#endif
