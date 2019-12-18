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
 * libpmemobj/pool_base.h -- definitions of libpmemobj pool entry points
 */

#ifndef LIBPMEMOBJ_POOL_BASE_H
#define LIBPMEMOBJ_POOL_BASE_H 1

#include <stddef.h>
#include <sys/types.h>

#include <libpmemobj/base.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PMEMOBJ_MIN_POOL ((size_t)(1024 * 1024 * 8)) /* 8 MiB */

/*
 * This limit is set arbitrary to incorporate a pool header and required
 * alignment plus supply.
 */
#define PMEMOBJ_MIN_PART ((size_t)(1024 * 1024 * 2)) /* 2 MiB */

/*
 * Pool management.
 */
#ifdef _WIN32
#ifndef PMDK_UTF8_API
#define pmemobj_open pmemobj_openW
#define pmemobj_create pmemobj_createW
#define pmemobj_check pmemobj_checkW
#else
#define pmemobj_open pmemobj_openU
#define pmemobj_create pmemobj_createU
#define pmemobj_check pmemobj_checkU
#endif
#endif

#ifndef _WIN32
PMEMobjpool *pmemobj_open(const char *path, const char *layout);
#else
PMEMobjpool *pmemobj_openU(const char *path, const char *layout);
PMEMobjpool *pmemobj_openW(const wchar_t *path, const wchar_t *layout);
#endif

#ifndef _WIN32
PMEMobjpool *pmemobj_create(const char *path, const char *layout,
	size_t poolsize, mode_t mode);
#else
PMEMobjpool *pmemobj_createU(const char *path, const char *layout,
	size_t poolsize, mode_t mode);
PMEMobjpool *pmemobj_createW(const wchar_t *path, const wchar_t *layout,
	size_t poolsize, mode_t mode);
#endif

#ifndef _WIN32
int pmemobj_check(const char *path, const char *layout);
#else
int pmemobj_checkU(const char *path, const char *layout);
int pmemobj_checkW(const wchar_t *path, const wchar_t *layout);
#endif

void pmemobj_close(PMEMobjpool *pop);
/*
 * If called for the first time on a newly created pool, the root object
 * of given size is allocated.  Otherwise, it returns the existing root object.
 * In such case, the size must be not less than the actual root object size
 * stored in the pool.  If it's larger, the root object is automatically
 * resized.
 *
 * This function is thread-safe.
 */
PMEMoid pmemobj_root(PMEMobjpool *pop, size_t size);

/*
 * Same as above, but calls the constructor function when the object is first
 * created and on all subsequent reallocations.
 */
PMEMoid pmemobj_root_construct(PMEMobjpool *pop, size_t size,
	pmemobj_constr constructor, void *arg);

/*
 * Returns the size in bytes of the root object. Always equal to the requested
 * size.
 */
size_t pmemobj_root_size(PMEMobjpool *pop);

/*
 * Sets volatile pointer to the user data for specified pool.
 */
void pmemobj_set_user_data(PMEMobjpool *pop, void *data);

/*
 * Gets volatile pointer to the user data associated with the specified pool.
 */
void *pmemobj_get_user_data(PMEMobjpool *pop);

#ifdef __cplusplus
}
#endif
#endif	/* libpmemobj/pool_base.h */
