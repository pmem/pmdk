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
 * libpmemobj/base.h -- definitions of base libpmemobj entry points
 */

#ifndef LIBPMEMOBJ_BASE_H
#define LIBPMEMOBJ_BASE_H 1

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#include <pmemcompat.h>

#ifndef PMDK_UTF8_API
#define pmemobj_check_version pmemobj_check_versionW
#define pmemobj_errormsg pmemobj_errormsgW
#else
#define pmemobj_check_version pmemobj_check_versionU
#define pmemobj_errormsg pmemobj_errormsgU
#endif

#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * opaque type internal to libpmemobj
 */
typedef struct pmemobjpool PMEMobjpool;

#define PMEMOBJ_MAX_ALLOC_SIZE ((size_t)0x3FFDFFFC0)

/*
 * allocation functions flags
 */
#define POBJ_FLAG_ZERO			(((uint64_t)1) << 0)
#define POBJ_FLAG_NO_FLUSH		(((uint64_t)1) << 1)
#define POBJ_FLAG_NO_SNAPSHOT		(((uint64_t)1) << 2)
#define POBJ_FLAG_ASSUME_INITIALIZED	(((uint64_t)1) << 3)
#define POBJ_FLAG_TX_NO_ABORT		(((uint64_t)1) << 4)

#define POBJ_CLASS_ID(id)	(((uint64_t)(id)) << 48)
#define POBJ_ARENA_ID(id)	(((uint64_t)(id)) << 32)

#define POBJ_XALLOC_CLASS_MASK	((((uint64_t)1 << 16) - 1) << 48)
#define POBJ_XALLOC_ARENA_MASK	((((uint64_t)1 << 16) - 1) << 32)
#define POBJ_XALLOC_ZERO	POBJ_FLAG_ZERO
#define POBJ_XALLOC_NO_FLUSH	POBJ_FLAG_NO_FLUSH
#define POBJ_XALLOC_NO_ABORT	POBJ_FLAG_TX_NO_ABORT

/*
 * pmemobj_mem* flags
 */
#define PMEMOBJ_F_MEM_NODRAIN		(1U << 0)

#define PMEMOBJ_F_MEM_NONTEMPORAL	(1U << 1)
#define PMEMOBJ_F_MEM_TEMPORAL		(1U << 2)

#define PMEMOBJ_F_MEM_WC		(1U << 3)
#define PMEMOBJ_F_MEM_WB		(1U << 4)

#define PMEMOBJ_F_MEM_NOFLUSH		(1U << 5)

/*
 * pmemobj_mem*, pmemobj_xflush & pmemobj_xpersist flags
 */
#define PMEMOBJ_F_RELAXED		(1U << 31)

/*
 * Persistent memory object
 */

/*
 * Object handle
 */
typedef struct pmemoid {
	uint64_t pool_uuid_lo;
	uint64_t off;
} PMEMoid;

static const PMEMoid OID_NULL = { 0, 0 };
#define OID_IS_NULL(o)	((o).off == 0)
#define OID_EQUALS(lhs, rhs)\
((lhs).off == (rhs).off &&\
	(lhs).pool_uuid_lo == (rhs).pool_uuid_lo)

PMEMobjpool *pmemobj_pool_by_ptr(const void *addr);
PMEMobjpool *pmemobj_pool_by_oid(PMEMoid oid);

#ifndef _WIN32

extern int _pobj_cache_invalidate;
extern __thread struct _pobj_pcache {
	PMEMobjpool *pop;
	uint64_t uuid_lo;
	int invalidate;
} _pobj_cached_pool;

/*
 * Returns the direct pointer of an object.
 */
static inline void *
pmemobj_direct_inline(PMEMoid oid)
{
	if (oid.off == 0 || oid.pool_uuid_lo == 0)
		return NULL;

	struct _pobj_pcache *cache = &_pobj_cached_pool;
	if (_pobj_cache_invalidate != cache->invalidate ||
			cache->uuid_lo != oid.pool_uuid_lo) {
		cache->invalidate = _pobj_cache_invalidate;

		if (!(cache->pop = pmemobj_pool_by_oid(oid))) {
			cache->uuid_lo = 0;
			return NULL;
		}

		cache->uuid_lo = oid.pool_uuid_lo;
	}

	return (void *)((uintptr_t)cache->pop + oid.off);
}

#endif /* _WIN32 */

/*
 * Returns the direct pointer of an object.
 */
#if defined(_WIN32) || defined(_PMEMOBJ_INTRNL) ||\
	defined(PMEMOBJ_DIRECT_NON_INLINE)
void *pmemobj_direct(PMEMoid oid);
#else
#define pmemobj_direct pmemobj_direct_inline
#endif

struct pmemvlt {
	uint64_t runid;
};

#define PMEMvlt(T)\
struct {\
	struct pmemvlt vlt;\
	T value;\
}

/*
 * Returns lazily initialized volatile variable. (EXPERIMENTAL)
 */
void *pmemobj_volatile(PMEMobjpool *pop, struct pmemvlt *vlt,
	void *ptr, size_t size,
	int (*constr)(void *ptr, void *arg), void *arg);

/*
 * Returns the OID of the object pointed to by addr.
 */
PMEMoid pmemobj_oid(const void *addr);

/*
 * Returns the number of usable bytes in the object. May be greater than
 * the requested size of the object because of internal alignment.
 *
 * Can be used with objects allocated by any of the available methods.
 */
size_t pmemobj_alloc_usable_size(PMEMoid oid);

/*
 * Returns the type number of the object.
 */
uint64_t pmemobj_type_num(PMEMoid oid);

/*
 * Pmemobj specific low-level memory manipulation functions.
 *
 * These functions are meant to be used with pmemobj pools, because they provide
 * additional functionality specific to this type of pool. These may include
 * for example replication support. They also take advantage of the knowledge
 * of the type of memory in the pool (pmem/non-pmem) to assure persistence.
 */

/*
 * Pmemobj version of memcpy. Data copied is made persistent.
 */
void *pmemobj_memcpy_persist(PMEMobjpool *pop, void *dest, const void *src,
	size_t len);

/*
 * Pmemobj version of memset. Data range set is made persistent.
 */
void *pmemobj_memset_persist(PMEMobjpool *pop, void *dest, int c, size_t len);

/*
 * Pmemobj version of memcpy. Data copied is made persistent (unless opted-out
 * using flags).
 */
void *pmemobj_memcpy(PMEMobjpool *pop, void *dest, const void *src, size_t len,
		unsigned flags);

/*
 * Pmemobj version of memmove. Data copied is made persistent (unless opted-out
 * using flags).
 */
void *pmemobj_memmove(PMEMobjpool *pop, void *dest, const void *src, size_t len,
		unsigned flags);

/*
 * Pmemobj version of memset. Data range set is made persistent (unless
 * opted-out using flags).
 */
void *pmemobj_memset(PMEMobjpool *pop, void *dest, int c, size_t len,
		unsigned flags);

/*
 * Pmemobj version of pmem_persist.
 */
void pmemobj_persist(PMEMobjpool *pop, const void *addr, size_t len);

/*
 * Pmemobj version of pmem_persist with additional flags argument.
 */
int pmemobj_xpersist(PMEMobjpool *pop, const void *addr, size_t len,
		unsigned flags);

/*
 * Pmemobj version of pmem_flush.
 */
void pmemobj_flush(PMEMobjpool *pop, const void *addr, size_t len);

/*
 * Pmemobj version of pmem_flush with additional flags argument.
 */
int pmemobj_xflush(PMEMobjpool *pop, const void *addr, size_t len,
		unsigned flags);

/*
 * Pmemobj version of pmem_drain.
 */
void pmemobj_drain(PMEMobjpool *pop);

/*
 * Version checking.
 */

/*
 * PMEMOBJ_MAJOR_VERSION and PMEMOBJ_MINOR_VERSION provide the current version
 * of the libpmemobj API as provided by this header file.  Applications can
 * verify that the version available at run-time is compatible with the version
 * used at compile-time by passing these defines to pmemobj_check_version().
 */
#define PMEMOBJ_MAJOR_VERSION 2
#define PMEMOBJ_MINOR_VERSION 4

#ifndef _WIN32
const char *pmemobj_check_version(unsigned major_required,
	unsigned minor_required);
#else
const char *pmemobj_check_versionU(unsigned major_required,
	unsigned minor_required);
const wchar_t *pmemobj_check_versionW(unsigned major_required,
	unsigned minor_required);
#endif

/*
 * Passing NULL to pmemobj_set_funcs() tells libpmemobj to continue to use the
 * default for that function.  The replacement functions must not make calls
 * back into libpmemobj.
 */
void pmemobj_set_funcs(
		void *(*malloc_func)(size_t size),
		void (*free_func)(void *ptr),
		void *(*realloc_func)(void *ptr, size_t size),
		char *(*strdup_func)(const char *s));

typedef int (*pmemobj_constr)(PMEMobjpool *pop, void *ptr, void *arg);

/*
 * (debug helper function) logs notice message if used inside a transaction
 */
void _pobj_debug_notice(const char *func_name, const char *file, int line);

#ifndef _WIN32
const char *pmemobj_errormsg(void);
#else
const char *pmemobj_errormsgU(void);
const wchar_t *pmemobj_errormsgW(void);
#endif

#ifdef __cplusplus
}
#endif
#endif	/* libpmemobj/base.h */
