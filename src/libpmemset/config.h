/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2020-2021, Intel Corporation */

/*
 * config.h -- internal definitions for pmemset_config
 */
#ifndef PMEMSET_CONFIG_H
#define PMEMSET_CONFIG_H

#include <stdbool.h>
#include "libpmemset.h"
struct pmemset_config;

void pmemset_config_init(struct pmemset_config *cfg);
int pmemset_config_duplicate(struct pmemset_config **cfg_out,
				struct pmemset_config *cfg_in);
enum pmem2_granularity pmemset_get_config_granularity(
	struct pmemset_config *cfg);
bool pmemset_get_config_granularity_valid(
	struct pmemset_config *cfg);
void pmemset_config_set_contiguous_part_coalescing(
		struct pmemset_config *cfg, bool value);
bool pmemset_config_get_contiguous_part_coalescing(
		struct pmemset_config *cfg);

int
pmemset_config_event_callback(struct pmemset_config *cfg,
	struct pmemset *set, struct pmemset_event_context *ctx);
#endif /* PMEMSET_CONFIG_H */
