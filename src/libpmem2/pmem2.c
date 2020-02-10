// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

/*
 * pmem2.c -- pmem2 entry points for libpmem2
 */

#include "libpmem2.h"

int
pmem2_config_set_sharing(struct pmem2_config *cfg, unsigned type)
{
	return PMEM2_E_NOSUPP;
}

int
pmem2_config_set_protection(struct pmem2_config *cfg, unsigned flag)
{
	return PMEM2_E_NOSUPP;
}

int
pmem2_config_set_address(struct pmem2_config *cfg, unsigned type, void *addr)
{
	return PMEM2_E_NOSUPP;
}

pmem2_memmove_fn
pmem2_get_memmove_fn(struct pmem2_map *map)
{
	return NULL;
}

pmem2_memcpy_fn
pmem2_get_memcpy_fn(struct pmem2_map *map)
{
	return NULL;
}

pmem2_memset_fn
pmem2_get_memset_fn(struct pmem2_map *map)
{
	return NULL;
}
