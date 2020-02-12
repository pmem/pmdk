// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

/*
 * config.c -- pmem2_config implementation
 */

#include <unistd.h>
#include "alloc.h"
#include "config.h"
#include "libpmem2.h"
#include "out.h"
#include "pmem2.h"
#include "pmem2_utils.h"

/*
 * pmem2_config_init -- initialize cfg structure.
 */
void
pmem2_config_init(struct pmem2_config *cfg)
{
#ifdef _WIN32
	cfg->handle = INVALID_HANDLE_VALUE;
#else
	cfg->fd = INVALID_FD;
#endif
	cfg->offset = 0;
	cfg->length = 0;
	cfg->alignment = 0;
	cfg->requested_max_granularity = PMEM2_GRANULARITY_INVALID;
}

/*
 * pmem2_config_new -- allocates and initialize cfg structure.
 */
int
pmem2_config_new(struct pmem2_config **cfg)
{
	int ret;
	*cfg = pmem2_malloc(sizeof(**cfg), &ret);

	if (ret)
		return ret;

	ASSERTne(cfg, NULL);

	pmem2_config_init(*cfg);
	return 0;
}

/*
 * pmem2_config_delete -- deallocate cfg structure.
 */
int
pmem2_config_delete(struct pmem2_config **cfg)
{
	Free(*cfg);
	*cfg = NULL;
	return 0;
}

/*
 * pmem2_config_set_required_store_granularity -- set granularity
 * requested by user in the pmem2_config structure
 */
int
pmem2_config_set_required_store_granularity(struct pmem2_config *cfg,
		enum pmem2_granularity g)
{
	switch (g) {
		case PMEM2_GRANULARITY_BYTE:
		case PMEM2_GRANULARITY_CACHE_LINE:
		case PMEM2_GRANULARITY_PAGE:
			break;
		default:
			ERR("unknown granularity value %d", g);
			return PMEM2_E_INVALID_ARG;
	}

	cfg->requested_max_granularity = g;

	return 0;
}

/*
 * pmem2_config_set_offset -- set offset in the pmem2_config structure
 */
int
pmem2_config_set_offset(struct pmem2_config *cfg, size_t offset)
{
	/* mmap func takes offset as a type of off_t */
	if (offset > (size_t)INT64_MAX) {
		ERR("offset is greater than INT64_MAX");
		return PMEM2_E_OFFSET_OUT_OF_RANGE;
	}

	cfg->offset = offset;

	return 0;
}

/*
 * pmem2_config_set_length -- set length in the pmem2_config structure
 */
int
pmem2_config_set_length(struct pmem2_config *cfg, size_t length)
{
	cfg->length = length;

	return 0;
}

/*
 * pmem2_config_validate_length -- validate that length in the pmem2_config
 * structure is consistent with the file length
 */
int
pmem2_config_validate_length(const struct pmem2_config *cfg,
		size_t file_len)
{
	size_t alignment;
	int ret = pmem2_config_get_alignment(cfg, &alignment);
	if (ret)
		return ret;

	ASSERTne(alignment, 0);
	if (cfg->length % alignment) {
		ERR("length is not a multiple of %lu", alignment);
		return PMEM2_E_LENGTH_UNALIGNED;
	}

	/* overflow check */
	const size_t end = cfg->offset + cfg->length;
	if (end < cfg->offset) {
		ERR("overflow of offset and length");
		return PMEM2_E_MAP_RANGE;
	}

	/* let's align the file size */
	size_t aligned_file_len = file_len;
	if (file_len % alignment)
		aligned_file_len = ALIGN_UP(file_len, alignment);

	/* validate mapping fit into the file */
	if (end > aligned_file_len) {
		ERR("mapping larger than file size");
		return PMEM2_E_MAP_RANGE;
	}

	return 0;
}
