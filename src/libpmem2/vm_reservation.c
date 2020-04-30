// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * vm_reservation.c -- implementation of virtual memory allocation API
 */

#include "libpmem2.h"

/*
 * pmem2_vm_reservation_new -- creates new virtual memory reservation
 */
int
pmem2_vm_reservation_new(struct pmem2_vm_reservation **rsv,
		size_t size, void *address)
{
	return PMEM2_E_NOSUPP;
}

/*
 * pmem2_vm_reservation_delete -- deletes reservation bound to
 *                                structure pmem2_vm_reservation
 */
int
pmem2_vm_reservation_delete(struct pmem2_vm_reservation **rsv)
{
	return PMEM2_E_NOSUPP;
}
