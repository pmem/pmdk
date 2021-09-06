/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2015-2020, Intel Corporation */

/*
 * heap_layout.h -- internal definitions for heap layout
 */

#ifndef LIBPMEMOBJ_HEAP_LAYOUT_H
#define LIBPMEMOBJ_HEAP_LAYOUT_H 1

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HEAP_MAJOR 1
#define HEAP_MINOR 0

#define MAX_CHUNK (UINT16_MAX - 7) /* has to be multiple of 8 */
#define CHUNK_BASE_ALIGNMENT 1024
#define CHUNKSIZE ((size_t)1024 * 256)	/* 256 kilobytes */
#define MAX_MEMORY_BLOCK_SIZE (MAX_CHUNK * CHUNKSIZE)
#define HEAP_SIGNATURE_LEN 16
#define HEAP_SIGNATURE "MEMORY_HEAP_HDR\0"
#define ZONE_HEADER_MAGIC 0xC3F0A2D2
#define ZONE_MIN_SIZE (sizeof(struct zone) + sizeof(struct chunk))
#define ZONE_MAX_SIZE (sizeof(struct zone) + sizeof(struct chunk) * MAX_CHUNK)
#define HEAP_MIN_SIZE (sizeof(struct heap_layout) + ZONE_MIN_SIZE)

/* Base bitmap values, relevant for both normal and flexible bitmaps */
#define RUN_BITS_PER_VALUE 64U
#define RUN_BASE_METADATA_VALUES\
	((unsigned)(sizeof(struct chunk_run_header) / sizeof(uint64_t)))
#define RUN_BASE_METADATA_SIZE (sizeof(struct chunk_run_header))

#define RUN_CONTENT_SIZE (CHUNKSIZE - RUN_BASE_METADATA_SIZE)

/*
 * Calculates the size in bytes of a single run instance, including bitmap
 */
#define RUN_CONTENT_SIZE_BYTES(size_idx)\
(RUN_CONTENT_SIZE + (((size_idx) - 1) * CHUNKSIZE))

/* Default bitmap values, specific for old, non-flexible, bitmaps */
#define RUN_DEFAULT_METADATA_VALUES 40 /* in 8 byte words, 320 bytes total */
#define RUN_DEFAULT_BITMAP_VALUES \
	(RUN_DEFAULT_METADATA_VALUES - RUN_BASE_METADATA_VALUES)
#define RUN_DEFAULT_BITMAP_SIZE (sizeof(uint64_t) * RUN_DEFAULT_BITMAP_VALUES)
#define RUN_DEFAULT_BITMAP_NBITS\
	(RUN_BITS_PER_VALUE * RUN_DEFAULT_BITMAP_VALUES)
#define RUN_DEFAULT_SIZE \
	(CHUNKSIZE - RUN_BASE_METADATA_SIZE - RUN_DEFAULT_BITMAP_SIZE)

/*
 * Calculates the size in bytes of a single run instance, without bitmap,
 * but only for the default fixed-bitmap algorithm
 */
#define RUN_DEFAULT_SIZE_BYTES(size_idx)\
(RUN_DEFAULT_SIZE + (((size_idx) - 1) * CHUNKSIZE))

#define CHUNK_MASK ((CHUNKSIZE) - 1)
#define CHUNK_ALIGN_UP(value) ((((value) + CHUNK_MASK) & ~CHUNK_MASK))

enum chunk_flags {
	CHUNK_FLAG_COMPACT_HEADER	=	0x0001,
	CHUNK_FLAG_HEADER_NONE		=	0x0002,
	CHUNK_FLAG_ALIGNED		=	0x0004,
	CHUNK_FLAG_FLEX_BITMAP		=	0x0008,
};

#define CHUNK_FLAGS_ALL_VALID (\
	CHUNK_FLAG_COMPACT_HEADER |\
	CHUNK_FLAG_HEADER_NONE |\
	CHUNK_FLAG_ALIGNED |\
	CHUNK_FLAG_FLEX_BITMAP\
)

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

struct chunk_run_header {
	uint64_t block_size;
	uint64_t alignment; /* valid only /w CHUNK_FLAG_ALIGNED */
};

struct chunk_run {
	struct chunk_run_header hdr;
	uint8_t content[RUN_CONTENT_SIZE]; /* bitmap + data */
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

#define ALLOC_HDR_COMPACT_SIZE sizeof(struct allocation_header_compact)

struct allocation_header_compact {
	uint64_t size;
	uint64_t extra;
};

enum header_type {
	HEADER_LEGACY,
	HEADER_COMPACT,
	HEADER_NONE,

	MAX_HEADER_TYPES
};

static const size_t header_type_to_size[MAX_HEADER_TYPES] = {
	sizeof(struct allocation_header_legacy),
	sizeof(struct allocation_header_compact),
	0
};

static const enum chunk_flags header_type_to_flag[MAX_HEADER_TYPES] = {
	(enum chunk_flags)0,
	CHUNK_FLAG_COMPACT_HEADER,
	CHUNK_FLAG_HEADER_NONE
};

static inline struct zone *
ZID_TO_ZONE(struct heap_layout *layout, size_t zone_id)
{
	return (struct zone *)
		((uintptr_t)&layout->zone0 + ZONE_MAX_SIZE * zone_id);
}

static inline struct chunk_header *
GET_CHUNK_HDR(struct heap_layout *layout, size_t zone_id, unsigned chunk_id)
{
	return &ZID_TO_ZONE(layout, zone_id)->chunk_headers[chunk_id];
}

static inline struct chunk *
GET_CHUNK(struct heap_layout *layout, size_t zone_id, unsigned chunk_id)
{
	return &ZID_TO_ZONE(layout, zone_id)->chunks[chunk_id];
}

static inline struct chunk_run *
GET_CHUNK_RUN(struct heap_layout *layout, size_t zone_id, unsigned chunk_id)
{
	return (struct chunk_run *)GET_CHUNK(layout, zone_id, chunk_id);
}

#ifdef __cplusplus
}
#endif

#endif
