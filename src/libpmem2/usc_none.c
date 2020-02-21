// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * usc_none.c -- pmem2 usc function for non supported platform
 */

#include "libpmem2.h"

#ifndef _WIN32
int
pmem2_source_device_id(const struct pmem2_source *src, char *id, size_t *len)
{
	return PMEM2_E_NOSUPP;
}
#else
int
pmem2_source_device_idW(const struct pmem2_source *src,
	wchar_t *id, size_t *len)
{
	return PMEM2_E_NOSUPP;
}

int
pmem2_source_device_idU(const struct pmem2_source *src, char *id, size_t *len)
{
	return PMEM2_E_NOSUPP;
}
#endif

int
pmem2_source_device_usc(const struct pmem2_source *src, uint64_t *usc)
{
	return PMEM2_E_NOSUPP;
}
