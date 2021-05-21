// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2021, Intel Corporation */

/*
 * config.c -- implementation of common config API
 */

#include "libpmem2.h"
#include "libpmemset.h"

#include "alloc.h"
#include "config.h"
#include "out.h"
#include "pmemset_utils.h"

/*
 * pmemset_config -- pmemset configuration structure.
 */
struct pmemset_config {
	bool set_granularity_valid;
	enum pmem2_granularity set_granularity;
	bool part_coalescing;
	pmemset_event_callback *callback;
	void *arg;
	struct pmem2_vm_reservation *set_reservation;
};

/*
 * pmemset_config_init -- initialize cfg structure.
 */
void
pmemset_config_init(struct pmemset_config *cfg)
{
	cfg->set_granularity_valid = false;
	cfg->part_coalescing = false;
	cfg->callback = NULL;
	cfg->arg = NULL;
	cfg->set_reservation = NULL;
}

/*
 * pmemset_get_config_granularity -- returns pmemset granularity value
 */
enum pmem2_granularity
pmemset_get_config_granularity(struct pmemset_config *cfg)
{
	ASSERTeq(cfg->set_granularity_valid, true);
	return cfg->set_granularity;
}

/*
 * pmemset_get_config_granularity_valid -- returns true if granularity
 * is set in the config
 */
bool
pmemset_get_config_granularity_valid(struct pmemset_config *cfg)
{
	return cfg->set_granularity_valid;
}

/*
 * pmemset_config_new -- allocates and initialize cfg structure.
 */
int
pmemset_config_new(struct pmemset_config **cfg)
{
	PMEMSET_ERR_CLR();

	int ret;
	*cfg = pmemset_malloc(sizeof(**cfg), &ret);

	if (ret)
		return ret;

	ASSERTne(cfg, NULL);

	pmemset_config_init(*cfg);

	return 0;
}

/*
 * pmemset_config_set_event_callback -- set an callback in the config
 */
void
pmemset_config_set_event_callback(struct pmemset_config *cfg,
		pmemset_event_callback *callback, void *arg)
{
	cfg->callback = callback;
	cfg->arg = arg;
}

/*
 * pmemset_config_event_callback -- call user provided callback
 */
int
pmemset_config_event_callback(struct pmemset_config *cfg,
	struct pmemset *set, struct pmemset_event_context *ctx)
{
	if (!cfg->callback)
		return 0;

	return cfg->callback(set, ctx, cfg->arg);
}

/*
 * pmemset_config_set_reservation -- set virtual memory reservation in the
 *                                   config
 */
void
pmemset_config_set_reservation(struct pmemset_config *cfg,
		struct pmem2_vm_reservation *rsv)
{
	LOG(3, "config %p reservation %p", cfg, rsv);

	cfg->set_reservation = rsv;
}

/*
 * pmemset_config_get_reservation -- get virtual memory reservation from the
 *                                   config
 */
struct pmem2_vm_reservation *
pmemset_config_get_reservation(struct pmemset_config *config)
{
	return config->set_reservation;
}

/*
 * pmemset_config_set_contiguous_part_coalescing -- sets the part coalescing
 * flag in the config to the provided value.
 */
void
pmemset_config_set_contiguous_part_coalescing(
		struct pmemset_config *cfg, bool value)
{
	cfg->part_coalescing = value;
}

/*
 * pmemset_config_get_contiguous_part_coalescing -- returns the part coalescing
 *                                                  flag value from the config
 */
bool
pmemset_config_get_contiguous_part_coalescing(
		struct pmemset_config *cfg)
{
	return cfg->part_coalescing;
}

/*
 * pmemset_config_set_required_store_granularity -- set granularity for pmemset
 */
int
pmemset_config_set_required_store_granularity(struct pmemset_config *cfg,
		enum pmem2_granularity g)
{
	PMEMSET_ERR_CLR();

	switch (g) {
		case PMEM2_GRANULARITY_BYTE:
		case PMEM2_GRANULARITY_CACHE_LINE:
		case PMEM2_GRANULARITY_PAGE:
			break;
		default:
			ERR("unknown granularity value %d", g);
			return PMEMSET_E_GRANULARITY_NOT_SUPPORTED;
	}

	cfg->set_granularity = g;
	cfg->set_granularity_valid = true;

	return 0;
}
/*
 * pmemset_config_delete -- deallocate cfg structure
 */
int
pmemset_config_delete(struct pmemset_config **cfg)
{
	Free(*cfg);
	*cfg = NULL;
	return 0;
}

/*
 * pmemset_config_duplicate -- copy cfg structure, allocate destination if
 *                             needed.
 */
int
pmemset_config_duplicate(struct pmemset_config **cfg_dst,
			struct pmemset_config *cfg_src)
{
	int ret;

	/* Allocate dst if needed */
	if (*cfg_dst == NULL) {
		*cfg_dst = pmemset_malloc(sizeof(**cfg_dst), &ret);
		if (ret)
			return ret;
	}

	/* Copy cfg */
	(*cfg_dst)->set_granularity = cfg_src->set_granularity;
	(*cfg_dst)->set_granularity_valid = cfg_src->set_granularity_valid;
	(*cfg_dst)->set_reservation = cfg_src->set_reservation;
	(*cfg_dst)->callback = cfg_src->callback;
	(*cfg_dst)->arg = cfg_src->arg;

	return 0;
}
