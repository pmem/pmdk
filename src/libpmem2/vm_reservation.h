/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2020-2023, Intel Corporation */

/*
 * vm_reservation.h -- internal definitions for virtual memory reservation
 */
#ifndef PMEM2_VM_RESERVATION_H
#define PMEM2_VM_RESERVATION_H

#include "ravl_interval.h"

struct pmem2_vm_reservation;

int vm_reservation_map_register_release(struct pmem2_vm_reservation *rsv,
		struct pmem2_map *map);
int vm_reservation_map_unregister_release(struct pmem2_vm_reservation *rsv,
		struct pmem2_map *map);
struct pmem2_map *vm_reservation_map_find_acquire(
		struct pmem2_vm_reservation *rsv, size_t reserv_offset,
		size_t len);
void vm_reservation_release(struct pmem2_vm_reservation *rsv);
int vm_reservation_extend_memory(struct pmem2_vm_reservation *rsv, void *addr,
		size_t size);
int vm_reservation_shrink_memory(struct pmem2_vm_reservation *rsv,
		void *rsv_release_addr, size_t size);

size_t vm_reservation_get_map_alignment(size_t len, size_t min_align);

#endif /* vm_reservation.h */
