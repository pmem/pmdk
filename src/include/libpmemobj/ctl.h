/*
 * Copyright 2017, Intel Corporation
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
 * The following set of structs/ctl entry points allows one to fully customize
 * internal data structures of the allocator.
 *
 * When requesting an object from the allocator, the first step is to determine
 * which allocation class best approximates the size of the object.
 * Once found, the appropriate free list, called bucket, for that
 * class is selected in a fashion that minimizes contention between threads.
 * Depending on the requested size and the configuration of the size<>class
 * mapping, it might happen that the object size (including required metadata)
 * would be bigger than the allocation class size - called unit size. In those
 * situations, the object is constructed from two or more units (up to 64).
 *
 * If the requested number of units cannot be retrieved from the selected
 * bucket, the thread reaches out to the global, shared, heap which manages
 * memory in 256 kilobyte chunks and gives it out in a best-fit fashion. This
 * operation must be performed under an exlusive lock.
 * Once the thread is in the possesion of a chunk, the lock is dropped, and the
 * memory is split into units that repopulate the bucket.
 *
 * These are the CTL entry points that control allocation classes:
 * - heap.alloc_class.reset
 *	Deletes all existing allocation classes and associated resources,
 *	allowing applications to have complete control over the allocator.
 *
 * - heap.alloc_class.[class_id].desc
 *	Creates/retrieves allocation class information
 *
 * - heap.alloc_class.map.range
 *	Creates a size<>alloc class mapping
 *
 * - heap.alloc_class.map.limit
 *	Retrieves the limit (in bytes) of the map
 *
 * - heap.alloc_class.map.granularity
 *	Retrieves the granularity (in bytes) of the map
 *
 * It's VERY important to remember that the allocation classes are a RUNTIME
 * property of the allocator - they are NOT stored persistently in the pool.
 * It's recommended to always create custom allocation classes and the
 * corresponding mapping immediately after creating and opening the pool,
 * before any use.
 * If there are existing objects created using a class that is no longer stored
 * in the runtime state of the allocator, they can be normally freed, but
 * allocating equivalent objects will be done using the allocation class that
 * is currently defined for that size.
 *
 * Please see the libpmemobj man page for more information.
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
	 * ensures that allocations are allocated in memory contiguously and
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
 * Archetype of allocation classes
 */
struct pobj_alloc_class_desc {
	/*
	 * The number of bytes in a single unit of allocation. A single
	 * allocation can span up to 64 units (or 1 in the case of minimal hdr).
	 * If one creates an allocation class with a certain unit size and
	 * forces it to handle bigger sizes, than more than one unit
	 * will be used.
	 * For example, an allocation class with a compact header and 128 bytes
	 * unit size, for a request of 200 bytes will create a memory block
	 * containing 256 bytes that spans two units. The usable size of that
	 * allocation will be 240 bytes: 2 * 128 - 16 (header).
	 */
	size_t unit_size;

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

/*
 * Memory range supported by an allocation class
 *
 * This memory range is written into a single global array which holds the
 * mapping between the size of an allocation request and the corresponding
 * allocation class.
 *
 * When specifying a range for the allocation classes, one must take the
 * size of the header into account.
 * For example, for an allocation class /w unit size of 128 bytes and a compact
 * header, the size '128' will be handled using two units, because the real
 * minimal size of that request is 144: 128 + 16 (header size). And so, it might
 * be better to create a specific allocation class /w unit size 144 and assign
 * it to handle the request for 128 bytes.
 */
struct pobj_alloc_class_map_range {
	/*
	 * A size in bytes that denotes the beginning of the range. Will be
	 * rounded up to the granularity of the map.
	 */
	size_t start;

	/*
	 * The size in bytes that denotes the end of the range.
	 */
	size_t end;

	/*
	 * The identifier of the allocation class that will handle the above
	 * specified range of memory.
	 */
	uint8_t class_id;
};

/*
 * Parameters of the allocation classes in general.
 * Can only be set when recreating the entire class collection.
 */
struct pobj_alloc_class_params {
	/*
	 * The limit in bytes of the allocation class map. This should be
	 * equal to the highest range that will be set.
	 */

	size_t limit;

	/*
	 * The granularity with which the map can be set. The bigger it is the
	 * less volatile memory is used for maintaining the map, but the
	 * precision is smaller. In other words, this is the size to which
	 * all the allocation sizes are aligned to when looking for a class.
	 *
	 * It does not make sense to set this to a value smaller than the
	 * smallest allocation class (including header).
	 */
	size_t granularity;

	/*
	 * Specifies whether the persistent allocations should fail if no
	 * allocation class is specified for the requested size.
	 * If not set, the default allocation class will handle those requests.
	 */
	int fail_no_matching_class;
};

#ifndef _WIN32
int pmemobj_ctl_get(PMEMobjpool *pop, const char *name, void *arg);
int pmemobj_ctl_set(PMEMobjpool *pop, const char *name, void *arg);
#else
int pmemobj_ctl_getU(PMEMobjpool *pop, const char *name, void *arg);
int pmemobj_ctl_getW(PMEMobjpool *pop, const wchar_t *name, void *arg);

int pmemobj_ctl_setU(PMEMobjpool *pop, const char *name, void *arg);
int pmemobj_ctl_setW(PMEMobjpool *pop, const wchar_t *name, void *arg);

#ifndef NVML_UTF8_API
#define pmemobj_ctl_get pmemobj_ctl_getW
#define pmemobj_ctl_set pmemobj_ctl_setW
#else
#define pmemobj_ctl_get pmemobj_ctl_getU
#define pmemobj_ctl_set pmemobj_ctl_setU
#endif

#endif


#ifdef __cplusplus
}
#endif
#endif	/* libpmemobj/ctl.h */
