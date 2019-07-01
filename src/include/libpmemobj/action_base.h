/*
 * Copyright 2017-2019, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
