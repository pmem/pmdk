/*
 * Copyright 2014-2016, Intel Corporation
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
 * convert_obj_v1_v2.c -- pmempool convert command source file
 */

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <endian.h>
#include "convert.h"

#define PMEMOBJ_MAX_LAYOUT ((size_t)1024)


struct arch_flags {
	uint64_t alignment_desc;	/* alignment descriptor */
	uint8_t ei_class;		/* ELF format file class */
	uint8_t ei_data;		/* ELF format data encoding */
	uint8_t reserved[4];
	uint16_t e_machine;		/* required architecture */
};

/*
 * header used at the beginning of all types of memory pools
 *
 * for pools build on persistent memory, the integer types
 * below are stored in little-endian byte order.
 */
#define POOL_HDR_SIG_LEN 8
#define POOL_HDR_UUID_LEN	16 /* uuid byte length */

typedef unsigned char uuid_t[POOL_HDR_UUID_LEN]; /* 16 byte binary uuid value */

struct pool_hdr {
	char signature[POOL_HDR_SIG_LEN];
	uint32_t major;			/* format major version number */
	uint32_t compat_features;	/* mask: compatible "may" features */
	uint32_t incompat_features;	/* mask: "must support" features */
	uint32_t ro_compat_features;	/* mask: force RO if unsupported */
	uuid_t poolset_uuid; /* pool set UUID */
	uuid_t uuid; /* UUID of this file */
	uuid_t prev_part_uuid; /* prev part */
	uuid_t next_part_uuid; /* next part */
	uuid_t prev_repl_uuid; /* prev replica */
	uuid_t next_repl_uuid; /* next replica */
	uint64_t crtime;		/* when created (seconds since epoch) */
	struct arch_flags arch_flags;	/* architecture identification flags */
	unsigned char unused[3944];	/* must be zero */
	uint64_t checksum;		/* checksum of above fields */
};

struct pmemobjpool {
	struct pool_hdr hdr;	/* memory pool header */

	/* persistent part of PMEMOBJ pool descriptor (2kB) */
	char layout[PMEMOBJ_MAX_LAYOUT];
	uint64_t lanes_offset;
	uint64_t nlanes;
	uint64_t heap_offset;
	uint64_t heap_size;
	/* the rest is irrelevant */
};

#define LANE_SECTION_LEN 1024

enum lane_section_type {
	LANE_SECTION_ALLOCATOR,
	LANE_SECTION_LIST,
	LANE_SECTION_TRANSACTION,

	MAX_LANE_SECTION
};

struct redo_log {
	uint64_t offset;	/* offset with finish flag */
	uint64_t value;
};

#define REDO_NUM_ENTRIES \
((LANE_SECTION_LEN - 2 * sizeof(uint64_t)) / sizeof(struct redo_log))
#define REDO_LOG_SIZE 4
struct allocator_lane_section {
	struct redo_log redo[REDO_LOG_SIZE];
};

struct lane_list_section {
	uint64_t obj_offset;
	struct redo_log redo[REDO_NUM_ENTRIES];
};

typedef struct pmemoid {
	uint64_t pool_uuid_lo;
	uint64_t off;
} PMEMoid;

typedef struct pmemmutex {
	char data[64];
} PMEMmutex;

struct list_entry {
	PMEMoid pe_next;
	PMEMoid pe_prev;
};

struct list_head {
	PMEMoid pe_first;
	PMEMmutex lock;
};

struct lane_tx_layout {
	uint64_t state;
	struct list_head undo_alloc;
	struct list_head undo_free;
	struct list_head undo_set;
	struct list_head undo_set_cache;
};

struct lane_section_layout {
	unsigned char data[LANE_SECTION_LEN];
};

struct lane_layout {
	struct lane_section_layout sections[MAX_LANE_SECTION];
};

struct allocation_header {
	uint32_t zone_id;
	uint32_t chunk_id;
	uint64_t size;
};

struct oob_header {
	struct list_entry oob;

	/* used only in root object, last bit used as a mask */
	size_t size;

	uint64_t type_num;
};

struct object {
	struct allocation_header alloch;
	struct oob_header oobh;
	unsigned char data[];
};

enum tx_state {
	TX_STATE_NONE = 0,
	TX_STATE_COMMITTED = 1,
};

struct tx_range {
	uint64_t offset;
	uint64_t size;
	uint8_t data[];
};

#define MAX_CACHED_RANGE_SIZE 32
#define MAX_CACHED_RANGES 169

struct tx_range_cache {
	struct { /* compatible with struct tx_range */
		uint64_t offset;
		uint64_t size;
		uint8_t data[MAX_CACHED_RANGE_SIZE];
	} range[MAX_CACHED_RANGES];
};

#define REDO_FINISH_FLAG	((uint64_t)1<<0)
#define REDO_FLAG_MASK		(~REDO_FINISH_FLAG)

static struct pmemobjpool *pop;
static struct heap_layout *heap;

#define D_RW pmemobj_direct

#define D_RW_OBJ(_oid)\
((struct object *)((uint64_t)D_RW((_oid)) - sizeof(struct object)))

#define BITS_PER_VALUE 64U
#define MAX_CACHELINE_ALIGNMENT 40 /* run alignment, 5 cachelines */

#define CHUNKSIZE ((size_t)1024 * 256)	/* 256 kilobytes */
#define MAX_CHUNK (UINT16_MAX - 7) /* has to be multiple of 8 */
#define RUN_METASIZE (MAX_CACHELINE_ALIGNMENT * 8)
#define MAX_BITMAP_VALUES (MAX_CACHELINE_ALIGNMENT - 2)
#define RUNSIZE (CHUNKSIZE - RUN_METASIZE)

enum chunk_flags {
	CHUNK_FLAG_ZEROED	=	0x0001,
	CHUNK_RUN_ACTIVE	=	0x0002
};

enum chunk_type {
	CHUNK_TYPE_UNKNOWN,
	CHUNK_TYPE_FOOTER, /* not actual chunk type */
	CHUNK_TYPE_FREE,
	CHUNK_TYPE_USED,
	CHUNK_TYPE_RUN,

	MAX_CHUNK_TYPE
};

struct chunk {
	uint8_t data[CHUNKSIZE];
};

struct chunk_run {
	uint64_t block_size;
	uint64_t bucket_vptr; /* runtime information */
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

struct memory_block {
	uint32_t chunk_id;
	uint32_t zone_id;
	uint32_t size_idx;
	uint16_t block_off;
};

struct heap_header {
	unsigned char data[1024];
};


struct heap_layout {
	struct heap_header header;
	struct zone zone0;
};

#define ZONE_MAX_SIZE (sizeof(struct zone) + sizeof(struct chunk) * MAX_CHUNK)

#define ZID_TO_ZONE(layoutp, zone_id)\
	((struct zone *)((uintptr_t)&(((struct heap_layout *)(layoutp))->zone0)\
					+ ZONE_MAX_SIZE * (zone_id)))

#define CALC_SIZE_IDX(_unit_size, _size)\
((uint32_t)(((_size - 1) / _unit_size) + 1))

static void *
pmemobj_direct(PMEMoid oid)
{
	return (char *)pop + oid.off;
}

static size_t
redo_log_nflags(struct redo_log *redo, size_t nentries)
{
	size_t ret = 0;
	size_t i;

	for (i = 0; i < nentries; i++) {
		if (redo[i].offset & REDO_FINISH_FLAG)
			ret++;
	}

	return ret;
}

static void
redo_recover(struct redo_log *redo, size_t nentries)
{
	size_t nflags = redo_log_nflags(redo, nentries);
	if (nflags == 0)
		return;

	assert(nflags != 1);

	uint64_t *val;
	while ((redo->offset & REDO_FINISH_FLAG) == 0) {
		val = (uint64_t *)((uintptr_t)pop + redo->offset);
		*val = redo->value;

		redo++;
	}

	uint64_t offset = redo->offset & REDO_FLAG_MASK;
	val = (uint64_t *)((uintptr_t)pop + offset);
	*val = redo->value;
}

static int
pfree(uint64_t *off)
{
	uint64_t offset = *off;
	if (offset == 0)
		return 0;

	PMEMoid oid;
	oid.off = offset;

	struct allocation_header *hdr = &D_RW_OBJ(oid)->alloch;

	struct zone *z = ZID_TO_ZONE(heap, hdr->zone_id);
	struct chunk_header *chdr = &z->chunk_headers[hdr->chunk_id];
	if (chdr->type == CHUNK_TYPE_USED) {
		chdr->type = CHUNK_TYPE_FREE;
		return 0;
	} else if (chdr->type != CHUNK_TYPE_RUN) {
		assert(0);
	}

	struct chunk_run *run =
		(struct chunk_run *)&z->chunks[hdr->chunk_id].data;
	uintptr_t diff = (uintptr_t)hdr - (uintptr_t)&run->data;
	uint64_t block_off = (uint16_t)((size_t)diff / run->block_size);
	uint64_t size_idx = CALC_SIZE_IDX(run->block_size, hdr->size);

	uint64_t bmask = ((1ULL << size_idx) - 1ULL) <<
			(block_off % BITS_PER_VALUE);

	uint64_t bpos = block_off / BITS_PER_VALUE;

	run->bitmap[bpos] &= ~bmask;
	*off = 0;

	return 0;
}

static int
lane_alloc_recover(struct allocator_lane_section *alloc)
{
	redo_recover(alloc->redo, REDO_LOG_SIZE);

	return 0;
}

static int
lane_list_recover(struct lane_list_section *list)
{
	redo_recover(list->redo, REDO_NUM_ENTRIES);
	pfree(&list->obj_offset);

	return 0;
}

static void
foreach_clear_undo_list(struct list_head *head,
	void (*cb)(PMEMoid oid), int free)
{
	PMEMoid iter = head->pe_first;
	PMEMoid next = {0, 0};

	/*
	 * Ff the list is empty the condition will be true because next is
	 * initialized with zeroes - same as the first element of an empty list
	 */
	while (next.off != head->pe_first.off) {
		next = D_RW_OBJ(iter)->oobh.oob.pe_next;

		if (cb)
			cb(iter);

		if (free)
			pfree(&iter.off);
		else
			memset(&D_RW_OBJ(iter)->oobh.oob, 0,
				sizeof(struct list_entry));

		iter = next;
	};

	memset(head, 0, sizeof(*head));
}

static void
restore_range(struct tx_range *r)
{
	void *dest = (char *)pop + r->offset;
	memcpy(dest, r->data, r->size);
}

static void
restore_set_range(PMEMoid set)
{
	restore_range(D_RW(set));
}

static void
restore_set_cache_range(PMEMoid cache)
{
	struct tx_range_cache *c = D_RW(cache);
	struct tx_range *range = NULL;
	for (int i = 0; i < MAX_CACHED_RANGES; ++i) {
		range = (struct tx_range *)&c->range[i];
		if (range->offset == 0 || range->size == 0)
			break;

		restore_range(range);
	}
}

static int
lane_tx_abort(struct lane_tx_layout *tx)
{
	foreach_clear_undo_list(&tx->undo_alloc,
		NULL, 1);
	foreach_clear_undo_list(&tx->undo_free,
		NULL, 0);
	foreach_clear_undo_list(&tx->undo_set,
		restore_set_range, 1);
	foreach_clear_undo_list(&tx->undo_set_cache,
		restore_set_cache_range, 1);

	return 0;
}

static int
lane_tx_commit(struct lane_tx_layout *tx)
{
	foreach_clear_undo_list(&tx->undo_alloc, NULL, 0);
	foreach_clear_undo_list(&tx->undo_free, NULL, 1);
	foreach_clear_undo_list(&tx->undo_set, NULL, 1);
	foreach_clear_undo_list(&tx->undo_set_cache, NULL, 1);

	return 0;
}

static int
lane_tx_recover(struct lane_tx_layout *tx)
{
	if (tx->state == TX_STATE_NONE) { /* abort */
		return lane_tx_abort(tx);
	} else if (tx->state == TX_STATE_COMMITTED) {
		tx->state = TX_STATE_NONE;

		return lane_tx_commit(tx);
	} else {
		return -1;
	}
}

#define SOURCE_MAJOR_VERSION 1
#define TARGET_MAJOR_VERSION 2

static int
util_checksum(void *addr, size_t len, uint64_t *csump, int insert)
{
	uint32_t *p32 = addr;
	uint32_t *p32end = (uint32_t *)((char *)addr + len);
	uint32_t lo32 = 0;
	uint32_t hi32 = 0;
	uint64_t csum;

	while (p32 < p32end)
		if (p32 == (uint32_t *)csump) {
			/* lo32 += 0; treat first 32-bits as zero */
			p32++;
			hi32 += lo32;
			/* lo32 += 0; treat second 32-bits as zero */
			p32++;
			hi32 += lo32;
		} else {
			lo32 += le32toh(*p32);
			++p32;
			hi32 += lo32;
		}

	csum = (uint64_t)hi32 << 32 | lo32;

	if (insert) {
		*csump = htole64(csum);
		return 1;
	}

	return *csump == htole64(csum);
}

int
convert_v1_v2(void *addr)
{
	pop = addr;
	heap = (struct heap_layout *)((char *)addr + pop->heap_offset);
	if (le32toh(pop->hdr.major) != SOURCE_MAJOR_VERSION)
		return -1;

	pop->hdr.major = htole32(TARGET_MAJOR_VERSION);
	util_checksum(&pop->hdr, sizeof(pop->hdr), &pop->hdr.checksum, 1);

	struct lane_layout *lanes =
		(struct lane_layout *)((char *)addr + pop->lanes_offset);
	for (uint64_t i = 0; i < pop->nlanes; ++i) {
		lane_alloc_recover((struct allocator_lane_section *)
			&lanes[i].sections[LANE_SECTION_ALLOCATOR]);
		lane_list_recover((struct lane_list_section *)
			&lanes[i].sections[LANE_SECTION_LIST]);
		lane_tx_recover((struct lane_tx_layout *)
			&lanes[i].sections[LANE_SECTION_TRANSACTION]);
	}
	memset(lanes, 0, pop->nlanes * sizeof(struct lane_layout));

	return 0;
}
