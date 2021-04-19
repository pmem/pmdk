/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2017-2021, Intel Corporation */

/*
 * libpmemobj/ctl.h -- definitions of pmemobj_ctl related entry points
 */

#ifndef LIBPMEMOBJ_CTL_H
#define LIBPMEMOBJ_CTL_H 1

#include <stddef.h>
#include <sys/types.h>

#include <libpmemobj/base.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Allocation class interface
 *
 * When requesting an object from the allocator, the first step is to determine
 * which allocation class best approximates the size of the object.
 * Once found, the appropriate free list, called bucket, for that
 * class is selected in a fashion that minimizes contention between threads.
 * Depending on the requested size and the allocation class, it might happen
 * that the object size (including required metadata) would be bigger than the
 * allocation class size - called unit size. In those situations, the object is
 * constructed from two or more units (up to 64).
 *
 * If the requested number of units cannot be retrieved from the selected
 * bucket, the thread reaches out to the global, shared, heap which manages
 * memory in 256 kilobyte chunks and gives it out in a best-fit fashion. This
 * operation must be performed under an exclusive lock.
 * Once the thread is in the possession of a chunk, the lock is dropped, and the
 * memory is split into units that repopulate the bucket.
 *
 * These are the CTL entry points that control allocation classes:
 * - heap.alloc_class.[class_id].desc
 *	Creates/retrieves allocation class information
 *
 * It's VERY important to remember that the allocation classes are a RUNTIME
 * property of the allocator - they are NOT stored persistently in the pool.
 * It's recommended to always create custom allocation classes immediately after
 * creating or opening the pool, before any use.
 * If there are existing objects created using a class that is no longer stored
 * in the runtime state of the allocator, they can be normally freed, but
 * allocating equivalent objects will be done using the allocation class that
 * is currently defined for that size.
 *
 * Please see the libpmemobj man page for more information about entry points.
 */

/*
 * Persistent allocation header
 */
enum pobj_header_type {
	/*
	 * 64-byte header used up until the version 1.3 of the library,
	 * functionally equivalent to the compact header.
	 * It's not recommended to create any new classes with this header.
	 */
	POBJ_HEADER_LEGACY,
	/*
	 * 16-byte header used by the default allocation classes. All library
	 * metadata is by default allocated using this header.
	 * Supports type numbers and variably sized allocations.
	 */
	POBJ_HEADER_COMPACT,
	/*
	 * 0-byte header with metadata stored exclusively in a bitmap. This
	 * ensures that objects are allocated in memory contiguously and
	 * without attached headers.
	 * This can be used to create very small allocation classes, but it
	 * does not support type numbers.
	 * Additionally, allocations with this header can only span a single
	 * unit.
	 * Objects allocated with this header do show up when iterating through
	 * the heap using pmemobj_first/pmemobj_next functions, but have a
	 * type_num equal 0.
	 */
	POBJ_HEADER_NONE,

	MAX_POBJ_HEADER_TYPES
};

/*
 * Description of allocation classes
 */
struct pobj_alloc_class_desc {
	/*
	 * The number of bytes in a single unit of allocation. A single
	 * allocation can span up to 64 units (or 1 in the case of no header).
	 * If one creates an allocation class with a certain unit size and
	 * forces it to handle bigger sizes, more than one unit
	 * will be used.
	 * For example, an allocation class with a compact header and 128 bytes
	 * unit size, for a request of 200 bytes will create a memory block
	 * containing 256 bytes that spans two units. The usable size of that
	 * allocation will be 240 bytes: 2 * 128 - 16 (header).
	 */
	size_t unit_size;

	/*
	 * Desired alignment of objects from the allocation class.
	 * If non zero, must be a power of two and an even divisor of unit size.
	 *
	 * All allocation classes have default alignment
	 * of 64. User data alignment is affected by the size of a header. For
	 * compact one this means that the alignment is 48 bytes.
	 *
	 */
	size_t alignment;

	/*
	 * The minimum number of units that must be present in a
	 * single, contiguous, memory block.
	 * Those blocks (internally called runs), are fetched on demand from the
	 * heap. Accessing that global state is a serialization point for the
	 * allocator and thus it is imperative for performance and scalability
	 * that a reasonable amount of memory is fetched in a single call.
	 * Threads generally do not share memory blocks from which they
	 * allocate, but blocks do go back to the global heap if they are no
	 * longer actively used for allocation.
	 */
	unsigned units_per_block;

	/*
	 * The header of allocations that originate from this allocation class.
	 */
	enum pobj_header_type header_type;

	/*
	 * The identifier of this allocation class.
	 */
	unsigned class_id;
};

enum pobj_stats_enabled {
	POBJ_STATS_ENABLED_TRANSIENT,
	POBJ_STATS_ENABLED_BOTH,
	POBJ_STATS_ENABLED_PERSISTENT,
	POBJ_STATS_DISABLED,
};

enum pobj_arenas_assignment_type {
	POBJ_ARENAS_ASSIGNMENT_THREAD_KEY,
	POBJ_ARENAS_ASSIGNMENT_GLOBAL,
};

#ifndef _WIN32
/* EXPERIMENTAL */
int pmemobj_ctl_get(PMEMobjpool *pop, const char *name, void *arg);
int pmemobj_ctl_set(PMEMobjpool *pop, const char *name, void *arg);
int pmemobj_ctl_exec(PMEMobjpool *pop, const char *name, void *arg);
#else
int pmemobj_ctl_getU(PMEMobjpool *pop, const char *name, void *arg);
int pmemobj_ctl_getW(PMEMobjpool *pop, const wchar_t *name, void *arg);
int pmemobj_ctl_setU(PMEMobjpool *pop, const char *name, void *arg);
int pmemobj_ctl_setW(PMEMobjpool *pop, const wchar_t *name, void *arg);
int pmemobj_ctl_execU(PMEMobjpool *pop, const char *name, void *arg);
int pmemobj_ctl_execW(PMEMobjpool *pop, const wchar_t *name, void *arg);

#ifndef PMDK_UTF8_API
#define pmemobj_ctl_get pmemobj_ctl_getW
#define pmemobj_ctl_set pmemobj_ctl_setW
#define pmemobj_ctl_exec pmemobj_ctl_execW
#else
#define pmemobj_ctl_get pmemobj_ctl_getU
#define pmemobj_ctl_set pmemobj_ctl_setU
#define pmemobj_ctl_exec pmemobj_ctl_execU
#endif

#endif

#ifdef __cplusplus
}
#endif
#endif	/* libpmemobj/ctl.h */
