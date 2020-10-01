// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * part.c -- implementation of common part API
 */

#include "part.h"
#include "libpmemset.h"

/*
 * pmemset_part_descriptor_new -- not supported
 */
int
pmemset_part_descriptor_new(struct pmemset_part_descriptor **part,
		struct pmemset *set, struct pmemset_source *src, size_t offset,
		size_t length)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * memset_part_descriptor_pread_mcsafe -- not supported
 */
int
pmemset_part_descriptor_pread_mcsafe(struct pmemset_part_descriptor *part,
		void *dst, size_t size, size_t offset)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_part_descriptor_pwrite_mcsafe -- not supported
 */
int
pmemset_part_descriptor_pwrite_mcsafe(struct pmemset_part_descriptor *part,
		void *dst, size_t size, size_t offset)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_part_finalize -- not supported
 */
int
pmemset_part_finalize(struct pmemset *set,
		struct pmemset_part_descriptor *descriptor,
		const struct pmemset_header *header_in,
		struct pmemset_header *header_out,
		const struct pmemset_part_shutdown_state_data *sds_in,
		struct pmemset_part_shutdown_state_data *sds_out,
		enum pmemset_part_state *state)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_part_address -- not supported
 */
int
pmemset_part_address(struct pmemset_part *part)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_part_length -- not supported
 */
int
pmemset_part_length(struct pmemset_part *part)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_part_first -- not supported
 */
int
pmemset_part_first(struct pmemset *set, struct pmemset_part **part)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_part_next -- not supported
 */
int
pmemset_part_next(struct pmemset *set, struct pmemset_part **part)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_part_by_address -- not supported
 */
int
pmemset_part_by_address(struct pmemset *set, struct pmemset_part **part,
		void *addr)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_part_drop -- not supported
 */
int
pmemset_part_drop(struct pmemset_part *part)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_part_map_new -- not supported
 */
int
pmemset_part_map_new(struct pmemset_part_map **pmap, struct pmemset_part **part,
		const struct pmemset_header *header_in,
		struct pmemset_header *header_out,
		const struct pmemset_part_shutdown_state_data *sds_in,
		struct pmemset_part_shutdown_state_data *sds_out,
		enum pmemset_part_state *state)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_part_map_drop -- not supported
 */
int
pmemset_part_map_drop(struct pmemset_part_map **pmap)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_part_map_address -- not supported
 */
int
pmemset_part_map_address(struct pmemset_part_map *pmap)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_part_map_length -- not supported
 */
int
pmemset_part_map_length(struct pmemset_part_map *pmap)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_part_map_first -- not supported
 */
int
pmemset_part_map_first(struct pmemset *set, struct pmemset_part_map **pmap)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_part_map_next -- not supported
 */
int
pmemset_part_map_next(struct pmemset *set, struct pmemset_part_map **pmap)
{
	return PMEMSET_E_NOSUPP;
}
