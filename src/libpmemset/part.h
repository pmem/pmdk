/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2020-2021, Intel Corporation */

/*
 * part.h -- internal definitions for libpmemset part API
 */
#ifndef PMEMSET_PART_H
#define PMEMSET_PART_H

struct pmemset_part;
struct pmemset_part_map {
	struct pmemset_part_descriptor desc;
	struct pmem2_vm_reservation *pmem2_reserv;
	int refcount;
};

/*
 * Shutdown state data must be stored by the user externally for reliability.
 * This needs to be read by the user and given to the add part function so that
 * the current shutdown state can be compared with the old one.
 */
struct pmemset_part_shutdown_state_data {
	const char data[1024];
};

struct pmemset *pmemset_part_get_pmemset(struct pmemset_part *part);

int pmemset_part_map_new(struct pmemset_part_map **part_map,
		struct pmemset_part *part, enum pmem2_granularity gran,
		enum pmem2_granularity *mapping_gran,
		struct pmemset_part_map *prev_pmap,
		enum pmemset_coalescing part_coalescing);

void pmemset_part_map_delete(struct pmemset_part_map **part_map);

#endif /* PMEMSET_PART_H */
