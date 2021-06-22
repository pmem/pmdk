/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2021, Intel Corporation */

/*
 * sds.h -- internal definitions for SDS module
 */
#ifndef PMEMSET_SDS_H
#define PMEMSET_SDS_H

#include "os_thread.h"

struct pmemset_sds_record;
struct pmemset_sds_state;

int pmemset_sds_delete(struct pmemset_sds **sds_ptr);

int pmemset_sds_duplicate(struct pmemset_sds **sds_dst,
		struct pmemset_sds *sds_src);

int pmemset_sds_check_and_possible_refresh(struct pmemset_source *src,
		enum pmemset_part_state *state_ptr);

int pmemset_sds_state_new(struct pmemset_sds_state **state);

int pmemset_sds_state_delete(struct pmemset_sds_state **state);

int pmemset_sds_register_record(struct pmemset_sds *sds, struct pmemset *set,
		struct pmemset_source *src, struct pmem2_map *p2map);

struct pmemset_sds_record *
pmemset_sds_find_record(struct pmem2_map *map, struct pmemset *set);

int pmemset_sds_fire_sds_update_event(struct pmemset_sds *sds,
		struct pmemset *set, struct pmemset_config *cfg,
		struct pmemset_source *src);

int pmemset_sds_unregister_record_fire_event(
		struct pmemset_sds_record *sds_record, struct pmemset *set,
		struct pmemset_config *cfg);

#endif /* PMEMSET_SDS_H */
