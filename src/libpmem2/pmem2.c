// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019, Intel Corporation */

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
pmem2_config_use_anonymous_mapping(struct pmem2_config *cfg, unsigned on)
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

#ifndef _WIN32
int
pmem2_get_device_id(const struct pmem2_config *cfg, char *id, size_t *len)
{
	return PMEM2_E_NOSUPP;
}
#else
int
pmem2_get_device_idW(const struct pmem2_config *cfg, wchar_t *id, size_t *len)
{
	return PMEM2_E_NOSUPP;
}

int
pmem2_get_device_idU(const struct pmem2_config *cfg, char *id, size_t *len)
{
	return PMEM2_E_NOSUPP;
}
#endif

int
pmem2_get_device_usc(const struct pmem2_config *cfg, uint64_t *usc)
{
	return PMEM2_E_NOSUPP;
}

int
pmem2_badblock_iterator_new(const struct pmem2_config *cfg,
		struct pmem2_badblock_iterator **pbb)
{
	return PMEM2_E_NOSUPP;
}

int
pmem2_badblock_next(struct pmem2_badblock_iterator *pbb,
		struct pmem2_badblock *bb)
{
	return PMEM2_E_NOSUPP;
}

void pmem2_badblock_iterator_delete(
		struct pmem2_badblock_iterator **pbb)
{
}

int
pmem2_badblock_clear(const struct pmem2_config *cfg,
		const struct pmem2_badblock *bb)
{
	return PMEM2_E_NOSUPP;
}
