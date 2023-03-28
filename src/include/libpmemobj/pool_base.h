/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2014-2023, Intel Corporation */

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
WIN_DEPR_ATTR
PMEMobjpool *pmemobj_openU(const char *path, const char *layout);
WIN_DEPR_ATTR
PMEMobjpool *pmemobj_openW(const wchar_t *path, const wchar_t *layout);
#endif

#ifndef _WIN32
PMEMobjpool *pmemobj_create(const char *path, const char *layout,
	size_t poolsize, mode_t mode);
#else
WIN_DEPR_ATTR
PMEMobjpool *pmemobj_createU(const char *path, const char *layout,
	size_t poolsize, mode_t mode);
WIN_DEPR_ATTR
PMEMobjpool *pmemobj_createW(const wchar_t *path, const wchar_t *layout,
	size_t poolsize, mode_t mode);
#endif

#ifndef _WIN32
int pmemobj_check(const char *path, const char *layout);
#else
WIN_DEPR_ATTR
int pmemobj_checkU(const char *path, const char *layout);
WIN_DEPR_ATTR
int pmemobj_checkW(const wchar_t *path, const wchar_t *layout);
#endif

#ifdef _WIN32
WIN_DEPR_ATTR
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
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
PMEMoid pmemobj_root(PMEMobjpool *pop, size_t size);

/*
 * Same as above, but calls the constructor function when the object is first
 * created and on all subsequent reallocations.
 */
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
PMEMoid pmemobj_root_construct(PMEMobjpool *pop, size_t size,
	pmemobj_constr constructor, void *arg);

/*
 * Returns the size in bytes of the root object. Always equal to the requested
 * size.
 */
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
size_t pmemobj_root_size(PMEMobjpool *pop);

/*
 * Sets volatile pointer to the user data for specified pool.
 */
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
void pmemobj_set_user_data(PMEMobjpool *pop, void *data);

/*
 * Gets volatile pointer to the user data associated with the specified pool.
 */
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
void *pmemobj_get_user_data(PMEMobjpool *pop);

#ifdef __cplusplus
}
#endif
#endif	/* libpmemobj/pool_base.h */
