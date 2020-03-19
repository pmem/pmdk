// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2020, Intel Corporation */

/*
 * libpmemobj/action_base.h -- definitions of libpmemobj action interface
 */

#ifndef LIBPMEMOBJ_ACTION_BASE_H
#define LIBPMEMOBJ_ACTION_BASE_H 1

#include <libpmemobj/base.h>

#ifdef __cplusplus
extern "C" {
#endif

enum pobj_action_type {
	/* a heap action (e.g., alloc) */
	POBJ_ACTION_TYPE_HEAP,
	/* a single memory operation (e.g., value set)  */
	POBJ_ACTION_TYPE_MEM,

	POBJ_MAX_ACTION_TYPE
};

struct pobj_action_heap {
	/* offset to the element being freed/allocated */
	uint64_t offset;
	/* usable size of the element being allocated */
	uint64_t usable_size;
};

struct pobj_action {
	/*
	 * These fields are internal for the implementation and are not
	 * guaranteed to be stable across different versions of the API.
	 * Use with caution.
	 *
	 * This structure should NEVER be stored on persistent memory!
	 */
	enum pobj_action_type type;
	uint32_t data[3];
	union {
		struct pobj_action_heap heap;
		uint64_t data2[14];
	};
};

#define POBJ_ACTION_XRESERVE_VALID_FLAGS\
	(POBJ_XALLOC_CLASS_MASK |\
	POBJ_XALLOC_ARENA_MASK |\
	POBJ_XALLOC_ZERO)

PMEMoid pmemobj_reserve(PMEMobjpool *pop, struct pobj_action *act,
	size_t size, uint64_t type_num);
PMEMoid pmemobj_xreserve(PMEMobjpool *pop, struct pobj_action *act,
	size_t size, uint64_t type_num, uint64_t flags);
void pmemobj_set_value(PMEMobjpool *pop, struct pobj_action *act,
	uint64_t *ptr, uint64_t value);
void pmemobj_defer_free(PMEMobjpool *pop, PMEMoid oid, struct pobj_action *act);

int pmemobj_publish(PMEMobjpool *pop, struct pobj_action *actv,
	size_t actvcnt);
int pmemobj_tx_publish(struct pobj_action *actv, size_t actvcnt);
int pmemobj_tx_xpublish(struct pobj_action *actv, size_t actvcnt,
		uint64_t flags);

void pmemobj_cancel(PMEMobjpool *pop, struct pobj_action *actv, size_t actvcnt);

#ifdef __cplusplus
}
#endif

#endif /* libpmemobj/action_base.h */
