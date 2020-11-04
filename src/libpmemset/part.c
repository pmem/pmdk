// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * part.c -- implementation of common part API
 */

#include <fcntl.h>

#include "libpmemset.h"
#include "libpmem2.h"
#include "os.h"
#include "part.h"
#include "pmemset.h"
#include "pmemset_utils.h"
#include "ravl_interval.h"
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
	LOG(3, "part %p extra %p desc %p", part, extra, desc);
	PMEMSET_ERR_CLR();

	struct pmem2_map *map;
	struct pmem2_config *pmem2_cfg;

	struct pmemset_part *p = *part;
	struct pmemset_source *s = p->src;
	struct pmemset *set = (*part)->set;
	int ret = 0;

	ret = pmem2_config_new(&pmem2_cfg);
	if (ret) {
		ERR("cannot create pmem2_config %d", ret);
		return PMEMSET_E_CANNOT_ALLOCATE_INTERNAL_STRUCTURE;
	}

	ret = pmem2_config_set_length(pmem2_cfg, p->length);
	ASSERTeq(ret, 0);

	ret = pmem2_config_set_offset(pmem2_cfg, p->offset);
	if (ret) {
		ERR("invalid value of pmem2_config offset %d", ret);
		ret = PMEMSET_E_INVALID_OFFSET_VALUE;
		goto err;
	}

	/* XXX: use pmemset_require_granularity function here */
	ret = pmem2_config_set_required_store_granularity(pmem2_cfg,
			PMEM2_GRANULARITY_PAGE);
	if (ret) {
		ERR("granularity value is not supported %d", ret);
		ret = PMEMSET_E_GRANULARITY_NOT_SUPPORTED;
		goto err;
	}

	enum pmemset_source_type type = pmemset_source_get_type(s);
	switch (type) {
		case PMEMSET_SOURCE_PMEM2:
			ret = pmemset_source_get_pmem2_map_from_src(
					s, pmem2_cfg, &map);
			if (ret)
				goto err;
			break;
		case PMEMSET_SOURCE_PATH:
			ret = pmemset_source_get_pmem2_map_from_file(
				s, pmem2_cfg, &map);
			if (ret)
				goto err;
			break;
		default:
			ERR("unrecognized pmemset source type");
			return PMEMSET_E_INVALID_SOURCE_TYPE;
	}

	struct pmemset_part_map *part_map;
	part_map = pmemset_malloc(sizeof(*part_map), &ret);
	if (ret)
		goto err;

	part_map->pmem2_map = map;

	/*
	 * XXX: add multiple part support
	 * Look at pmemset_part TEST8 - double pmemset_part_map,
	 * at this moment we have always different map from pmem2
	 */
	struct ravl_interval *pmt = pmemset_get_part_map_tree(set);
	ret = ravl_interval_insert(pmt, part_map);
	if (ret  == -EEXIST) {
		ERR("part already exists");
		ret = PMEMSET_E_PART_EXISTS;
	}

err:
	pmem2_config_delete(&pmem2_cfg);
	return ret;

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
