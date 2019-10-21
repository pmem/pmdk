/*
 * Copyright 2014-2019, Intel Corporation
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
 * libpmemobj/atomic_base.h -- definitions of libpmemobj atomic entry points
 */

#ifndef LIBPMEMOBJ_ATOMIC_BASE_H
#define LIBPMEMOBJ_ATOMIC_BASE_H 1

#include <libpmemobj/base.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Non-transactional atomic allocations
 *
 * Those functions can be used outside transactions. The allocations are always
 * aligned to the cache-line boundary.
 */

#define POBJ_XALLOC_VALID_FLAGS	(POBJ_XALLOC_ZERO |\
	POBJ_XALLOC_CLASS_MASK)

/*
 * Allocates a new object from the pool and calls a constructor function before
 * returning. It is guaranteed that allocated object is either properly
 * initialized, or if it's interrupted before the constructor completes, the
 * memory reserved for the object is automatically reclaimed.
 */
int pmemobj_alloc(PMEMobjpool *pop, PMEMoid *oidp, size_t size,
	uint64_t type_num, pmemobj_constr constructor, void *arg);

/*
 * Allocates with flags a new object from the pool.
 */
int pmemobj_xalloc(PMEMobjpool *pop, PMEMoid *oidp, size_t size,
	uint64_t type_num, uint64_t flags,
	pmemobj_constr constructor, void *arg);

/*
 * Allocates a new zeroed object from the pool.
 */
int pmemobj_zalloc(PMEMobjpool *pop, PMEMoid *oidp, size_t size,
	uint64_t type_num);

/*
 * Resizes an existing object.
 */
int pmemobj_realloc(PMEMobjpool *pop, PMEMoid *oidp, size_t size,
	uint64_t type_num);

/*
 * Resizes an existing object, if extended new space is zeroed.
 */
int pmemobj_zrealloc(PMEMobjpool *pop, PMEMoid *oidp, size_t size,
	uint64_t type_num);

/*
 * Allocates a new object with duplicate of the string s.
 */
int pmemobj_strdup(PMEMobjpool *pop, PMEMoid *oidp, const char *s,
	uint64_t type_num);

/*
 * Allocates a new object with duplicate of the wide character string s.
 */
int pmemobj_wcsdup(PMEMobjpool *pop, PMEMoid *oidp, const wchar_t *s,
	uint64_t type_num);

/*
 * Frees an existing object.
 */
void pmemobj_free(PMEMoid *oidp);

struct pobj_defrag_result {
	size_t total; /* number of processed objects */
	size_t relocated; /* number of relocated objects */
};

/*
 * Performs defragmentation on the provided array of objects.
 */
int pmemobj_defrag(PMEMobjpool *pop, PMEMoid **oidv, size_t oidcnt,
	struct pobj_defrag_result *result);

#ifdef __cplusplus
}
#endif

#endif	/* libpmemobj/atomic_base.h */
