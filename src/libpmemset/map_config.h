/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2021, Intel Corporation */

/*
 * map_config.h -- internal definitions for libpmemset part API
 */
#ifndef PMEMSET_MAP_CONFIG_H
#define PMEMSET_MAP_CONFIG_H

struct pmemset *pmemset_map_config_get_set(struct pmemset_map_config *map_cfg);
size_t pmemset_map_config_get_length(struct pmemset_map_config *map_cfg);
size_t pmemset_map_config_get_offset(struct pmemset_map_config *map_cfg);
struct pmemset_file *pmemset_map_config_get_file(
		struct pmemset_map_config *map_cfg);

#endif /* PMEMSET_MAP_CONFIG_H */
