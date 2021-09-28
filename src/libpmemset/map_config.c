// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/*
 * map_config.c -- implementation of common map_config API
 */

#include "alloc.h"
#include "libpmemset.h"
#include "map_config.h"
#include "pmemset.h"
#include "pmemset_utils.h"
#include "source.h"

struct pmemset_map_config {
	size_t offset;
	size_t length;
	struct pmemset_file *file;
};

int
pmemset_map_config_new(struct pmemset_map_config **map_cfg)
{
	LOG(3, "map_cfg %p", map_cfg);
	PMEMSET_ERR_CLR();

	int ret;
	struct pmemset_map_config *mapcfg;
	*map_cfg = NULL;

	mapcfg = pmemset_malloc(sizeof(*mapcfg), &ret);
	if (ret)
		return ret;

	ASSERTne(mapcfg, NULL);

	mapcfg->offset = 0;
	mapcfg->length = 0;
	*map_cfg = mapcfg;

	return ret;
}

/*
 * pmemset_map_config_set_offset -- sets offset in the map configuraton struct
 */
int
pmemset_map_config_set_offset(struct pmemset_map_config *map_cfg,
	size_t offset)
{
	LOG(3, "map_cfg %p offset %zu", map_cfg, offset);
	PMEMSET_ERR_CLR();

	/* mmap func takes offset as a type of off_t */
	if (offset > (size_t)INT64_MAX) {
		ERR("offset is greater than INT64_MAX");
		return PMEMSET_E_OFFSET_OUT_OF_RANGE;
	}

	map_cfg->offset = offset;

	return 0;
}

/*
 * pmemset_map_config_set_length -- sets length in the map configuraton struct
 */
void
pmemset_map_config_set_length(struct pmemset_map_config *map_cfg,
	size_t length)
{
	LOG(3, "map_cfg %p length %zu", map_cfg, length);

	map_cfg->length = length;
}

/*
 * pmemset_map_config_delete -- deletes pmemset map config
 */
int
pmemset_map_config_delete(struct pmemset_map_config **map_cfg)
{
	LOG(3, "map_cfg %p", map_cfg);
	PMEMSET_ERR_CLR();

	Free(*map_cfg);
	*map_cfg = NULL;

	return 0;
}

/*
 * pmemset_map_config_get_length -- return length assigned to the map config
 */
size_t
pmemset_map_config_get_length(struct pmemset_map_config *map_cfg)
{
	return map_cfg->length;
}

/*
 * pmemset_map_config_get_offset -- return offset assigned to the map config
 */
size_t
pmemset_map_config_get_offset(struct pmemset_map_config *map_cfg)
{
	return map_cfg->offset;
}
