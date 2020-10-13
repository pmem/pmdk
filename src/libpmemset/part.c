// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * part.c -- implementation of common part API
 */

#include <fcntl.h>

#include "libpmemset.h"
#include "os.h"
#include "part.h"
#include "pmemset.h"
#include "pmemset_utils.h"
#include "source.h"

struct pmemset_part {
	struct pmemset *set;
	struct pmemset_source *src;
	size_t offset;
	size_t length;
};

/*
 * pmemset_part_new -- creates a new part for the provided set
 */
int
pmemset_part_new(struct pmemset_part **part, struct pmemset *set,
		struct pmemset_source *src, size_t offset, size_t length)
{
	LOG(3, "part %p set %p src %p offset %zu length %zu",
			part, set, src, offset, length);
	PMEMSET_ERR_CLR();

	int ret = 0;
	struct pmemset_part *partp;
	*part = NULL;

	enum pmemset_source_type type = pmemset_source_get_type(src);
	switch (type) {
		case PMEMSET_SOURCE_PATH:;
			char *filepath;
			ret = pmemset_source_get_filepath(src,
					&filepath);
			if (ret)
				return ret;

			os_stat_t stat;
			if (os_stat(filepath, &stat) < 0) {
				if (errno == ENOENT) {
					ERR(
						"invalid path specified in the source");
					return PMEMSET_E_INVALID_FILE_PATH;
				}
				ERR("!stat");
				return PMEMSET_E_ERRNO;
			}
			break;
		case PMEMSET_SOURCE_PMEM2:;
			struct pmem2_source *pmem2_src;
			ret = pmemset_source_get_pmem2_source(src, &pmem2_src);
			if (ret)
				return ret;

			if (!pmem2_src)
				return PMEMSET_E_INVALID_PMEM2_SOURCE;
			break;
		default:
			ERR("unrecognized source type");
			return PMEMSET_E_INVALID_SOURCE_TYPE;
	}

	partp = pmemset_malloc(sizeof(*partp), &ret);
	if (ret)
		return ret;

	ASSERTne(partp, NULL);

	partp->set = set;
	partp->src = src;
	partp->offset = offset;
	partp->length = length;

	*part = partp;

	return 0;
}

/*
 * memset_part_pread_mcsafe -- not supported
 */
int
pmemset_part_pread_mcsafe(struct pmemset_part_descriptor *part,
		void *dst, size_t size, size_t offset)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_part_pwrite_mcsafe -- not supported
 */
int
pmemset_part_pwrite_mcsafe(struct pmemset_part_descriptor *part,
		void *dst, size_t size, size_t offset)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_part_map -- not supported
 */
int
pmemset_part_map(struct pmemset_part **part, struct pmemset_extras *extra,
		struct pmemset_part_descriptor *desc)
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
 * pmemset_part_map_descriptor -- not supported
 */
struct pmemset_part_descriptor
pmemset_part_map_descriptor(struct pmemset_part_map *pmap)
{
	struct pmemset_part_descriptor desc;
	/* compiler is crying when struct is uninitialized */
	desc.addr = NULL;
	desc.size = 0;
	return desc;
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

/*
 * pmemset_part_map_by_address -- not supported
 */
int
pmemset_part_map_by_address(struct pmemset *set, struct pmemset_part **part,
		void *addr)
{
	return PMEMSET_E_NOSUPP;
}
