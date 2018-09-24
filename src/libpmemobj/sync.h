/*
 * Copyright 2016-2018, Intel Corporation
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
 * sync.h -- internal to obj synchronization API
 */

#ifndef LIBPMEMOBJ_SYNC_H
#define LIBPMEMOBJ_SYNC_H 1

#include <errno.h>
#include <stdint.h>

#include "libpmemobj.h"
#include "out.h"
#include "os_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * internal definitions of PMEM-locks
 */
typedef union padded_pmemmutex {
	char padding[_POBJ_CL_SIZE];
	struct {
		uint64_t runid;
		union {
			os_mutex_t mutex;
			struct {
				void *bsd_mutex_p;
				union padded_pmemmutex *next;
			} bsd_u;
		} mutex_u;
	} pmemmutex;
} PMEMmutex_internal;
#define PMEMmutex_lock pmemmutex.mutex_u.mutex
#define PMEMmutex_bsd_mutex_p pmemmutex.mutex_u.bsd_u.bsd_mutex_p
#define PMEMmutex_next pmemmutex.mutex_u.bsd_u.next

typedef union padded_pmemrwlock {
	char padding[_POBJ_CL_SIZE];
	struct {
		uint64_t runid;
		union {
			os_rwlock_t rwlock;
			struct {
				void *bsd_rwlock_p;
				union padded_pmemrwlock *next;
			} bsd_u;
		} rwlock_u;
	} pmemrwlock;
} PMEMrwlock_internal;
#define PMEMrwlock_lock pmemrwlock.rwlock_u.rwlock
#define PMEMrwlock_bsd_rwlock_p pmemrwlock.rwlock_u.bsd_u.bsd_rwlock_p
#define PMEMrwlock_next pmemrwlock.rwlock_u.bsd_u.next

typedef union padded_pmemcond {
	char padding[_POBJ_CL_SIZE];
	struct {
		uint64_t runid;
		union {
			os_cond_t cond;
			struct {
				void *bsd_cond_p;
				union padded_pmemcond *next;
			} bsd_u;
		} cond_u;
	} pmemcond;
} PMEMcond_internal;
#define PMEMcond_cond pmemcond.cond_u.cond
#define PMEMcond_bsd_cond_p pmemcond.cond_u.bsd_u.bsd_cond_p
#define PMEMcond_next pmemcond.cond_u.bsd_u.next

/*
 * pmemobj_mutex_lock_nofail -- pmemobj_mutex_lock variant that never
 * fails from caller perspective. If pmemobj_mutex_lock failed, this function
 * aborts the program.
 */
static inline void
pmemobj_mutex_lock_nofail(PMEMobjpool *pop, PMEMmutex *mutexp)
{
	int ret = pmemobj_mutex_lock(pop, mutexp);
	if (ret) {
		errno = ret;
		FATAL("!pmemobj_mutex_lock");
	}
}

/*
 * pmemobj_mutex_unlock_nofail -- pmemobj_mutex_unlock variant that never
 * fails from caller perspective. If pmemobj_mutex_unlock failed, this function
 * aborts the program.
 */
static inline void
pmemobj_mutex_unlock_nofail(PMEMobjpool *pop, PMEMmutex *mutexp)
{
	int ret = pmemobj_mutex_unlock(pop, mutexp);
	if (ret) {
		errno = ret;
		FATAL("!pmemobj_mutex_unlock");
	}
}

int pmemobj_mutex_assert_locked(PMEMobjpool *pop, PMEMmutex *mutexp);

#ifdef __cplusplus
}
#endif

#endif
