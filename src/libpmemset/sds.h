/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2021, Intel Corporation */

/*
 * sds.h -- internal definitions for SDS module
 */
#ifndef PMEMSET_SDS_H
#define PMEMSET_SDS_H

struct pmemset_sds_record;

void pmemset_sds_init(void);

void pmemset_sds_fini(void);

int pmemset_sds_new(struct pmemset_sds **sds_ptr, struct pmemset_source *src);

int pmemset_sds_delete(struct pmemset_sds **sds_ptr);

int pmemset_sds_duplicate(struct pmemset_sds **sds_dst,
		struct pmemset_sds *sds_src);

int pmemset_sds_state_check_and_possible_refresh(struct pmemset_source *src,
		enum pmemset_part_state *state_ptr);

int pmemset_sds_register_record(struct pmemset_sds *sds,
		struct pmemset_source *src, struct pmem2_map *p2map);

int pmemset_sds_unregister_record(struct pmemset_sds_record *sds_record);

struct pmemset_sds_record *pmemset_sds_find_record(struct pmem2_map *map);

struct pmemset_sds *pmemset_sds_record_get_sds(
		struct pmemset_sds_record *sds_record);

struct pmemset_source *pmemset_sds_record_get_source(
		struct pmemset_sds_record *sds_record);

int pmemset_sds_fire_sds_update_event(struct pmemset *set,
		struct pmemset_sds *sds, struct pmemset_config *cfg,
		struct pmemset_source *src);

#endif /* PMEMSET_SDS_H */
