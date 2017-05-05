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
	 * metadata must be allocated using this header. Supports type numbers.
	 */
	POBJ_HEADER_COMPACT,
	/*
	 * 0-byte header with metadata stored exlusively in a bitmap. This
	 * ensures that allocations are allocated in memory contiguously and
	 * without explicit headers.
	 * This can be used to create very small allocation classes, but it
	 * does not support type numbers.
	 * Additionally, allocations with this header can only span a single
	 * unit.
	 */
	POBJ_HEADER_MINIMAL,

	MAX_POBJ_HEADER_TYPES
};

/*
 * Archetype of allocation classes
 */
struct pobj_alloc_class_proto {
	/*
	 * The number of bytes in a single unit of allocation. A single
	 * allocation can span up to 64 units (or 1 in the case of minimal hdr).
	 * If one creates an allocation class with a certain unit size and
	 * forces it to handle bigger sizes, than more than one unit
	 * will be used.
	 * For example, an allocation class with a compact header and 128 byte
	 * unit size, for a request of 200 bytes will create a memory block
	 * containing 256 bytes that spans two units. The usable size of that
	 * allocation will be 240 bytes: 2 * 128 - 16 (header).
	 */
	size_t unit_size;

	/*
	 * The minimum number of units present in a single instance of a
	 * run (a chunk subdivded into smaller blocks). The larger it is, the
	 * contention on the global heap is lower and allocator operations are
	 * faster.
	 * It will be rounded up so that the lowest possible amout of space
	 * is wasted.
	 */
	unsigned units_per_run;

	/*
	 * The header of allocations that originate from this allocation class.
	 */
	enum pobj_header_type header_type;
};

/*
 * Allocation class range of memory
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
	 * precision is smaller.
	 * It does not make sense to set this to a value smaller than the
	 * smallest allocation class (including header)/
	 */
	size_t granularity;

	/*
	 * Specifies whether the persistent allocations should fail if no
	 * allocation class is specified for the requested size.
	 * If not set, the default allocation class will handle those requests.
	 */
	int fail_on_missing_class;
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
