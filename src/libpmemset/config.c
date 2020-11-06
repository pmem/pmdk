// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * config.c -- implementation of common config API
 */

#include "libpmem2.h"
#include "libpmemset.h"

#include "alloc.h"
#include "config.h"
#include "event.h"
#include "out.h"
#include "pmemset_utils.h"

/*
 * pmemset_config -- pmemset configuration structure.
 */
struct pmemset_config {
	char stub;
};

/*
 * pmemset_config_init -- initialize cfg structure.
 */
void
pmemset_config_init(struct pmemset_config *cfg)
{
	cfg->stub = '\0';
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
 * pmemset_config_set_create_if_none -- not supported
 */
int
pmemset_config_set_create_if_none(struct pmemset_config *cfg, int value)
{
	ERR("function not supported");
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_config_set_create_if_invalid -- not supported
 */
int
pmemset_config_set_create_if_invalid(struct pmemset_config *cfg, int value)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_config_set_event_callback -- not supported
 */
int
pmemset_config_set_event_callback(struct pmemset_config *cfg,
		pmemset_event_callback *callback, void *arg)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_config_set_reservation -- not supported
 */
int
pmemset_config_set_reservation(struct pmemset_config *cfg,
		struct pmem2_vm_reservation *rsv)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_config_set_contiguous_part_coalescing -- not supported
 */
int
pmemset_config_set_contiguous_part_coalescing(struct pmemset_config *cfg,
		int value)
{
	return PMEMSET_E_NOSUPP;
}

#ifndef _WIN32
/*
 * pmemset_config_set_layout_name -- not supported
 */
int
pmemset_config_set_layout_name(struct pmemset_config *cfg,
		const char *layout)
{
	return PMEMSET_E_NOSUPP;
}
#else
/*
 * pmemset_config_set_layout_nameU -- not supported
 */
int
pmemset_config_set_layout_nameU(struct pmemset_config *cfg,
		const char *layout)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_config_set_layout_nameW -- not supported
 */
int
pmemset_config_set_layout_nameW(struct pmemset_config *cfg,
		const wchar_t *layout)
{
	return PMEMSET_E_NOSUPP;
}
#endif

/*
 * pmemset_config_set_version -- not supported
 */
int
pmemset_config_set_version(struct pmemset_config *cfg,
		int major, int minor)
{
	return PMEMSET_E_NOSUPP;
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
	(*cfg_dst)->stub = cfg_src->stub;

	return 0;
}
