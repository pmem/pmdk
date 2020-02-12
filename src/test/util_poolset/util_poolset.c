/*
 * Copyright 2015-2020, Intel Corporation
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
 * util_poolset.c -- unit test for util_pool_create() / util_pool_open()
 *
 * usage: util_poolset cmd minlen hdrsize [mockopts] setfile ...
 */

#include <stdbool.h>
#include "unittest.h"
#include "pmemcommon.h"
#include "set.h"
#include <errno.h>
#include "mocks.h"
#include "fault_injection.h"

#define LOG_PREFIX "ut"
#define LOG_LEVEL_VAR "TEST_LOG_LEVEL"
#define LOG_FILE_VAR "TEST_LOG_FILE"
#define MAJOR_VERSION 1
#define MINOR_VERSION 0
#define SIG "PMEMXXX"
#define MIN_PART ((size_t)(1024 * 1024 * 2)) /* 2 MiB */

#define TEST_FORMAT_INCOMPAT_DEFAULT	POOL_FEAT_CKSUM_2K
#define TEST_FORMAT_INCOMPAT_CHECK	POOL_FEAT_INCOMPAT_VALID

static size_t Extend_size = MIN_PART * 2;

const char *Open_path = "";
os_off_t Fallocate_len = -1;
size_t Is_pmem_len = 0;

/*
 * poolset_info -- (internal) dumps poolset info and checks its integrity
 *
 * Performs the following checks:
 * - part_size[i] == rounddown(file_size - pool_hdr_size, Mmap_align)
 * - replica_size == sum(part_size)
 * - pool_size == min(replica_size)
 */
static void
poolset_info(const char *fname, struct pool_set *set, int o)
{
	if (o)
		UT_OUT("%s: opened: nreps %d poolsize %zu rdonly %d",
			fname, set->nreplicas, set->poolsize,
			set->rdonly);
	else
		UT_OUT("%s: created: nreps %d poolsize %zu zeroed %d",
			fname, set->nreplicas, set->poolsize,
			set->zeroed);

	size_t poolsize = SIZE_MAX;

	for (unsigned r = 0; r < set->nreplicas; r++) {
		struct pool_replica *rep = set->replica[r];
		size_t repsize = 0;

		UT_OUT("  replica[%d]: nparts %d nhdrs %d repsize %zu "
				"is_pmem %d",
			r, rep->nparts, rep->nhdrs, rep->repsize, rep->is_pmem);

		for (unsigned i = 0; i < rep->nparts; i++) {
			struct pool_set_part *part = &rep->part[i];
			UT_OUT("    part[%d] path %s filesize %zu size %zu",
				i, part->path, part->filesize, part->size);
			size_t partsize =
				(part->filesize & ~(Ut_mmap_align - 1));
			repsize += partsize;
			if (i > 0 && (set->options & OPTION_SINGLEHDR) == 0)
				UT_ASSERTeq(part->size,
					partsize - Ut_mmap_align); /* XXX */
		}

		repsize -= (rep->nhdrs - 1) * Ut_mmap_align;
		UT_ASSERTeq(rep->repsize, repsize);
		UT_ASSERT(rep->resvsize >= repsize);

		if (rep->repsize < poolsize)
			poolsize = rep->repsize;
	}
	UT_ASSERTeq(set->poolsize, poolsize);
}

/*
 * mock_options -- (internal) parse mock options and enable mocked functions
 */
static int
mock_options(const char *arg)
{
	/* reset to defaults */
	Open_path = "";
	Fallocate_len = -1;
	Is_pmem_len = 0;

	if (arg[0] != '-' || arg[1] != 'm')
		return 0;

	switch (arg[2]) {
	case 'n':
		/* do nothing */
		break;
	case 'o':
		/* open */
		Open_path = &arg[4];
		break;
	case 'f':
		/* fallocate */
		Fallocate_len = ATOLL(&arg[4]);
		break;
	case 'p':
		/* is_pmem */
		Is_pmem_len = ATOULL(&arg[4]);
		break;
	default:
		UT_FATAL("unknown mock option: %c", arg[2]);
	}

	return 1;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "util_poolset");

	common_init(LOG_PREFIX, LOG_LEVEL_VAR, LOG_FILE_VAR,
			MAJOR_VERSION, MINOR_VERSION);

	if (argc < 3)
		UT_FATAL("usage: %s cmd minsize [mockopts] "
			"setfile ...", argv[0]);

	char *fname;
	struct pool_set *set;
	int ret;

	size_t minsize = strtoul(argv[2], &fname, 0);

	for (int arg = 3; arg < argc; arg++) {
		arg += mock_options(argv[arg]);
		fname = argv[arg];
		struct pool_attr attr;
		memset(&attr, 0, sizeof(attr));
		memcpy(attr.signature, SIG, sizeof(SIG));
		attr.major = 1;

		switch (argv[1][0]) {
		case 'c':
			attr.features.incompat = TEST_FORMAT_INCOMPAT_DEFAULT;
			ret = util_pool_create(&set, fname, 0, minsize,
				MIN_PART, &attr, NULL, REPLICAS_ENABLED);
			if (ret == -1)
				UT_OUT("!%s: util_pool_create", fname);
			else {
				/*
				 * XXX: On Windows pool files are created with
				 * R/W permissions, so no need for chmod().
				 */
#ifndef _WIN32
				util_poolset_chmod(set, S_IWUSR | S_IRUSR);
#endif
				poolset_info(fname, set, 0);
				util_poolset_close(set, DO_NOT_DELETE_PARTS);
			}
			break;
		case 'o':
			attr.features.incompat = TEST_FORMAT_INCOMPAT_CHECK;
			ret = util_pool_open(&set, fname, MIN_PART, &attr,
						NULL, NULL, 0 /* flags */);
			if (ret == -1)
				UT_OUT("!%s: util_pool_open", fname);
			else {
				poolset_info(fname, set, 1);
				util_poolset_close(set, DO_NOT_DELETE_PARTS);
			}
			break;
		case 'e':
			attr.features.incompat = TEST_FORMAT_INCOMPAT_CHECK;
			ret = util_pool_open(&set, fname, MIN_PART, &attr,
						NULL, NULL, 0 /* flags */);
			UT_ASSERTeq(ret, 0);
			size_t esize = Extend_size;
			void *nptr = util_pool_extend(set, &esize, MIN_PART);
			if (nptr == NULL)
				UT_OUT("!%s: util_pool_extend", fname);
			else {
				poolset_info(fname, set, 1);
			}
			util_poolset_close(set, DO_NOT_DELETE_PARTS);
			break;
		case 'f':
			if (!core_fault_injection_enabled())
				break;

			attr.features.incompat = TEST_FORMAT_INCOMPAT_CHECK;
			ret = util_pool_open(&set, fname, MIN_PART, &attr,
					NULL, NULL, 0 /* flags */);
			UT_ASSERTeq(ret, 0);
			size_t fsize = Extend_size;
			core_inject_fault_at(PMEM_MALLOC, 2,
					"util_poolset_append_new_part");
			void *fnptr = util_pool_extend(set, &fsize, MIN_PART);
			UT_ASSERTeq(fnptr, NULL);
			UT_ASSERTeq(errno, ENOMEM);
			util_poolset_close(set, DO_NOT_DELETE_PARTS);
			break;
		}
	}

	common_fini();

	DONE(NULL);
}
