/*
 * Copyright 2015-2017, Intel Corporation
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
 * heap_layout.h -- internal definitions for heap layout
 */

#ifndef LIBPMEMOBJ_HEAP_LAYOUT_H
#define LIBPMEMOBJ_HEAP_LAYOUT_H 1

#include <stddef.h>
#include <stdint.h>

#define HEAP_MAJOR 1
#define HEAP_MINOR 0

#define MAX_CHUNK (UINT16_MAX - 7) /* has to be multiple of 8 */
#define CHUNKSIZE ((size_t)1024 * 256)	/* 256 kilobytes */
#define MAX_MEMORY_BLOCK_SIZE (MAX_CHUNK * CHUNKSIZE)
#define HEAP_SIGNATURE_LEN 16
#define HEAP_SIGNATURE "MEMORY_HEAP_HDR\0"
#define ZONE_HEADER_MAGIC 0xC3F0A2D2
#define ZONE_MIN_SIZE (sizeof(struct zone) + sizeof(struct chunk))
#define ZONE_MAX_SIZE (sizeof(struct zone) + sizeof(struct chunk) * MAX_CHUNK)
#define HEAP_MIN_SIZE (sizeof(struct heap_layout) + ZONE_MIN_SIZE)

#define BITS_PER_VALUE 64U
#define MAX_CACHELINE_ALIGNMENT 40 /* run alignment, 5 cachelines */
#define RUN_METASIZE (MAX_CACHELINE_ALIGNMENT * 8)
#define MAX_BITMAP_VALUES (MAX_CACHELINE_ALIGNMENT - 2)
#define RUN_BITMAP_SIZE (BITS_PER_VALUE * MAX_BITMAP_VALUES)
#define RUNSIZE (CHUNKSIZE - RUN_METASIZE)
#define MIN_RUN_SIZE 128

#define ZID_TO_ZONE(layoutp, zone_id)\
	((struct zone *)((uintptr_t)&(((struct heap_layout *)(layoutp))->zone0)\
					+ ZONE_MAX_SIZE * (zone_id)))

#define CHUNK_MASK ((CHUNKSIZE) - 1)
#define CHUNK_ALIGN_UP(value) ((((value) + CHUNK_MASK) & ~CHUNK_MASK))

enum chunk_flags {
	CHUNK_FLAG_COMPACT_HEADER	=	0x0001,
	CHUNK_FLAG_HEADER_NONE		=	0x0002,
};

#define CHUNK_FLAGS_ALL_VALID (CHUNK_FLAG_COMPACT_HEADER |\
	CHUNK_FLAG_HEADER_NONE)

enum chunk_type {
	CHUNK_TYPE_UNKNOWN,
	CHUNK_TYPE_FOOTER, /* not actual chunk type */
	CHUNK_TYPE_FREE,
	CHUNK_TYPE_USED,
	CHUNK_TYPE_RUN,
	CHUNK_TYPE_RUN_DATA,

	MAX_CHUNK_TYPE
};

struct chunk {
	uint8_t data[CHUNKSIZE];
};

struct chunk_run {
	uint64_t block_size;
	uint64_t incarnation_claim; /* run_id of the last claimant */
	uint64_t bitmap[MAX_BITMAP_VALUES];
	uint8_t data[RUNSIZE];
};

struct chunk_header {
	uint16_t type;
	uint16_t flags;
	uint32_t size_idx;
};

struct zone_header {
	uint32_t magic;
	uint32_t size_idx;
	uint8_t reserved[56];
};

struct zone {
	struct zone_header header;
	struct chunk_header chunk_headers[MAX_CHUNK];
	struct chunk chunks[];
};

struct heap_header {
	char signature[HEAP_SIGNATURE_LEN];
	uint64_t major;
	uint64_t minor;
	uint64_t unused; /* might be garbage */
	uint64_t chunksize;
	uint64_t chunks_per_zone;
	uint8_t reserved[960];
	uint64_t checksum;
};

struct heap_layout {
	struct heap_header header;
	struct zone zone0;	/* first element of zones array */
};

#define ALLOC_HDR_SIZE_SHIFT (48ULL)
#define ALLOC_HDR_FLAGS_MASK (((1ULL) << ALLOC_HDR_SIZE_SHIFT) - 1)

struct allocation_header_legacy {
	uint8_t unused[8];
	uint64_t size;
	uint8_t unused2[32];
	uint64_t root_size;
	uint64_t type_num;
};

struct allocation_header_compact {
	uint64_t size;
	uint64_t extra;
};

#endif
