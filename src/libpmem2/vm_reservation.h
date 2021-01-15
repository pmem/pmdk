/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2020-2021, Intel Corporation */

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
struct pmem2_map *vm_reservation_map_find(struct pmem2_vm_reservation *rsv,
		size_t reserv_offset, size_t len);
struct pmem2_map *vm_reservation_map_find_acquire(
		struct pmem2_vm_reservation *rsv, size_t reserv_offset,
		size_t len);
void vm_reservation_release(struct pmem2_vm_reservation *rsv);
int vm_reservation_extend_memory(struct pmem2_vm_reservation *rsv,
		void *rsv_end_addr, size_t size);

#endif /* vm_reservation.h */
