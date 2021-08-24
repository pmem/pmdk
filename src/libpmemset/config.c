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

#define PMEMSET_PART_VALID_STATES (uint64_t)(PMEMSET_PART_STATE_INDETERMINATE |\
		PMEMSET_PART_STATE_OK | \
		PMEMSET_PART_STATE_OK_BUT_ALREADY_OPEN | \
		PMEMSET_PART_STATE_OK_BUT_INTERRUPTED | \
		PMEMSET_PART_STATE_CORRUPTED)

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
	uint64_t acceptable_states; /* default value  PMEMSET_PART_STATE_OK */
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
	cfg->acceptable_states = (PMEMSET_PART_STATE_OK |
			PMEMSET_PART_STATE_OK_BUT_ALREADY_OPEN);
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
	(*cfg_dst)->acceptable_states = cfg_src->acceptable_states;

	return 0;
}

/*
 * pmemset_config_set_acceptable_states -- sets acceptable part states that
 * should not return an error during mapping of a part.
 */
int
pmemset_config_set_acceptable_states(struct pmemset_config *cfg,
		uint64_t states)
{
	LOG(3, "config %p states %lu", cfg, states);
	PMEMSET_ERR_CLR();

	if (states & ~PMEMSET_PART_VALID_STATES)
		return PMEMSET_E_INVALID_PART_STATES;

	cfg->acceptable_states = states;

	return 0;
}

/*
 * pmemset_config_validate_state -- check if provided state is acceptable
 */
int
pmemset_config_validate_state(struct pmemset_config *cfg,
		enum pmemset_part_state state)
{
	if (state & ~cfg->acceptable_states) {
		ERR("part state %u doesn't match any acceptable state "\
				"set in config %p", state, cfg);
		return PMEMSET_E_UNDESIRABLE_PART_STATE;
	}

	return 0;
}
