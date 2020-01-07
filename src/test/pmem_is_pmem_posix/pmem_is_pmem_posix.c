// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2019, Intel Corporation */

/*
 * pmem_is_pmem_posix.c -- Posix specific unit test for pmem_is_pmem()
 *
 * usage: pmem_is_pmem_posix op addr len [op addr len ...]
 * where op can be: 'a' (add), 'r' (remove), 't' (test),
 * 'f' (fault_injection for util_range_register),
 * 's' (fault_injection for util_range_split)
 */

#include <stdlib.h>

#include "unittest.h"
#include "mmap.h"
#include "../libpmem/pmem.h"

static enum pmem_map_type
str2type(char *str)
{
	if (strcmp(str, "DEV_DAX") == 0)
		return PMEM_DEV_DAX;
	if (strcmp(str, "MAP_SYNC") == 0)
		return PMEM_MAP_SYNC;

	FATAL("unknown type '%s'", str);
}

static void
do_fault_injection_register(void *addr, size_t len, enum pmem_map_type type)
{
	if (!pmem_fault_injection_enabled())
		return;

	pmem_inject_fault_at(PMEM_MALLOC, 1, "util_range_register");

	int ret = util_range_register(addr, len, "", type);
	UT_ASSERTne(ret, 0);
	UT_ASSERTeq(errno, ENOMEM);
}

static void
do_fault_injection_split(void *addr, size_t len)
{
	if (!pmem_fault_injection_enabled())
		return;

	pmem_inject_fault_at(PMEM_MALLOC, 1, "util_range_split");

	int ret = util_range_unregister(addr, len);
	UT_ASSERTne(ret, 0);
	UT_ASSERTeq(errno, ENOMEM);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmem_is_pmem_posix");

	if (argc < 4)
		UT_FATAL("usage: %s op addr len type [op addr len type ...]",
				argv[0]);

	/* insert memory regions to the list */
	int i;
	for (i = 1; i < argc; ) {
		UT_ASSERT(i + 2 < argc);

		errno = 0;
		void *addr = (void *)strtoull(argv[i + 1], NULL, 0);
		UT_ASSERTeq(errno, 0);

		size_t len = strtoull(argv[i + 2], NULL, 0);
		UT_ASSERTeq(errno, 0);

		int ret;

		switch (argv[i][0]) {
		case 'a':
			ret = util_range_register(addr, len, "",
					str2type(argv[i + 3]));
			if (ret != 0)
				UT_OUT("%s", pmem_errormsg());
			i += 4;
			break;
		case 'r':
			ret = util_range_unregister(addr, len);
			UT_ASSERTeq(ret, 0);
			i += 3;
			break;
		case 't':
			UT_OUT("addr %p len %zu is_pmem %d",
					addr, len, pmem_is_pmem(addr, len));
			i += 3;
			break;
		case 'f':
			do_fault_injection_register(addr, len,
					str2type(argv[i + 3]));
			i += 4;
			break;
		case 's':
			do_fault_injection_split(addr, len);
			i += 3;
			break;
		default:
			FATAL("invalid op '%c'", argv[i][0]);
		}
	}

	DONE(NULL);
}
