/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2020-2021, Intel Corporation */

/*
 * part.h -- internal definitions for libpmemset part API
 */
#ifndef PMEMSET_PART_H
#define PMEMSET_PART_H

#include "file.h"

struct pmemset_part_map {
	struct pmemset *set;
	struct pmemset_source *src;
	struct pmem2_vm_reservation *pmem2_reserv;
	struct pmemset_part_descriptor desc;
	int refcount;
};

/*
 * typedef for callback function invoked on each iteration of pmem2 mapping
 * stored in the part mapping
 */
typedef int pmemset_part_map_iter_cb(struct pmemset_part_map *pmap,
		struct pmem2_map *map, void *arg);

/*
 * Shutdown state data must be stored by the user externally for reliability.
 * This needs to be read by the user and given to the add part function so that
 * the current shutdown state can be compared with the old one.
 */
struct pmemset_part_shutdown_state_data {
	const char data[1024];
};

int pmemset_part_map_new(struct pmemset_part_map **pmap_ptr,
		struct pmemset *set, struct pmemset_source *src,
		struct pmem2_vm_reservation *pmem2_reserv, size_t offset,
		size_t size);

int pmemset_part_map_delete(struct pmemset_part_map **pmap_ptr);

int pmemset_part_map_iterate(struct pmemset_part_map *pmap, size_t offset,
		size_t size, size_t *out_offset, size_t *out_size,
		pmemset_part_map_iter_cb cb, void *arg);

int pmemset_part_map_remove_range(struct pmemset_part_map *pmap, size_t offset,
		size_t size, size_t *out_offset, size_t *out_size);

int pmemset_part_file_try_ensure_size(struct pmemset_file *f,
		size_t len, size_t off, size_t source_size);

int pmemset_part_map_find(struct pmemset_part_map *pmap, size_t offset,
		size_t size, struct pmem2_map **p2map);

#endif /* PMEMSET_PART_H */
