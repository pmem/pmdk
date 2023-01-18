/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2019-2023, Intel Corporation */

/*
 * map.h -- internal definitions for libpmem2
 */
#ifndef PMEM2_MAP_H
#define PMEM2_MAP_H

#include <stddef.h>
#include <stdbool.h>
#include "libpmem2.h"
#include "os.h"
#include "source.h"
#include "libminiasync/vdm.h"
#include "vm_reservation.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*pmem2_deep_flush_fn)(struct pmem2_map *map,
		void *ptr, size_t size);

struct pmem2_map {
	void *addr; /* base address */
	size_t reserved_length; /* length of the mapping reservation */
	size_t content_length; /* length of the mapped content */
	/* effective persistence granularity */
	enum pmem2_granularity effective_granularity;

	pmem2_persist_fn persist_fn;
	pmem2_flush_fn flush_fn;
	pmem2_drain_fn drain_fn;
	pmem2_deep_flush_fn deep_flush_fn;

	pmem2_memmove_fn memmove_fn;
	pmem2_memcpy_fn memcpy_fn;
	pmem2_memset_fn memset_fn;

	struct pmem2_source source;
	struct pmem2_vm_reservation *reserv;

	struct vdm *vdm;
	bool custom_vdm;
};

enum pmem2_granularity get_min_granularity(bool eADR, bool is_pmem,
					enum pmem2_sharing_type sharing);
struct pmem2_map *pmem2_map_find(const void *addr, size_t len);
int pmem2_register_mapping(struct pmem2_map *map);
int pmem2_unregister_mapping(struct pmem2_map *map);
void pmem2_map_init(void);
void pmem2_map_fini(void);

int pmem2_validate_offset(const struct pmem2_config *cfg,
	size_t *offset, size_t alignment);

#ifdef __cplusplus
}
#endif

#endif /* map.h */
