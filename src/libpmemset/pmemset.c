// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * pmemset.c -- implementation of common pmemset API
 */

#include "pmemset.h"
#include "libpmemset.h"

/*
 * pmemset_new -- not supported
 */
int
pmemset_new(struct pmemset **set, struct pmemset_config *cfg)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_delete -- not supported
 */
void
pmemset_delete(struct pmemset **set)
{
	;
}

#ifndef _WIN32
/*
 * pmemset_header_init -- not supported
 */
int
pmemset_header_init(struct pmemset_header *header, const char *layout,
		int major, int minor)
{
	return PMEMSET_E_NOSUPP;
}
#else
/*
 * pmemset_header_initU -- not supported
 */
int
pmemset_header_initU(struct pmemset_header *header, const char *layout,
		int major, int minor)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_header_initW -- not supported
 */
int
pmemset_header_initW(struct pmemset_header *header, const wchar_t *layout,
		int major, int minor)
{
	return PMEMSET_E_NOSUPP;
}
#endif

/*
 * pmemset_remove_part -- not supported
 */
int
pmemset_remove_part(struct pmemset *set, struct pmemset_part **part)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_remove_part_map -- not supported
 */
void
pmemset_remove_part_map(struct pmemset *set, struct pmemset_part_map **part)
{
	;
}

/*
 * pmemset_remove_range -- not supported
 */
int
pmemset_remove_range(struct pmemset *set, void *addr, size_t len)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_persist -- not supported
 */
void
pmemset_persist(struct pmemset *set, const void *ptr, size_t size)
{
	;
}

/*
 * pmemset_flush -- not supported
 */
void
pmemset_flush(struct pmemset *set, const void *ptr, size_t size)
{
	;
}

/*
 * pmemset_drain -- not supported
 */
void
pmemset_drain(struct pmemset *set)
{
	;
}

/*
 * pmemset_memmove -- not supported
 */
void *
pmemset_memmove(struct pmemset *set, void *pmemdest, const void *src,
		size_t len, unsigned flags)
{
	return NULL;
}

/*
 * pmemset_memcpy -- not supported
 */
void *
pmemset_memcpy(struct pmemset *set, void *pmemdest, const void *src, size_t len,
		unsigned flags)
{
	return NULL;
}

/*
 * pmemset_memset -- not supported
 */
void *
pmemset_memset(struct pmemset *set, void *pmemdest, int c, size_t len,
		unsigned flags)
{
	return NULL;
}

/*
 * pmemset_deep_flush -- not supported
 */
int
pmemset_deep_flush(struct pmemset *set, void *ptr, size_t size)
{
	return PMEMSET_E_NOSUPP;
}
