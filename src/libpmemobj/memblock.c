/*
 * Copyright 2016-2019, Intel Corporation
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
 * memblock.c -- implementation of memory block
 *
 * Memory block is a representation of persistent object that resides in the
 * heap. A valid memory block must be either a huge (free or used) chunk or a
 * block inside a run.
 *
 * Huge blocks are 1:1 correlated with the chunk headers in the zone whereas
 * run blocks are represented by bits in corresponding chunk bitmap.
 *
 * This file contains implementations of abstract operations on memory blocks.
 * Instead of storing the mbops structure inside each memory block the correct
 * method implementation is chosen at runtime.
 */

#include <string.h>

#include "obj.h"
#include "heap.h"
#include "memblock.h"
#include "out.h"
#include "valgrind_internal.h"

/* calculates the size of the entire run, including any additional chunks */
#define SIZEOF_RUN(runp, size_idx)\
	(sizeof(*(runp)) + (((size_idx) - 1) * CHUNKSIZE))

/*
 * memblock_header_type -- determines the memory block's header type
 */
static enum header_type
memblock_header_type(const struct memory_block *m)
{
	struct chunk_header *hdr = heap_get_chunk_hdr(m->heap, m);

	if (hdr->flags & CHUNK_FLAG_COMPACT_HEADER)
		return HEADER_COMPACT;

	if (hdr->flags & CHUNK_FLAG_HEADER_NONE)
		return HEADER_NONE;

	return HEADER_LEGACY;
}

/*
 * memblock_header_legacy_get_size --
 *	(internal) returns the size stored in a legacy header
 */
static size_t
memblock_header_legacy_get_size(const struct memory_block *m)
{
	struct allocation_header_legacy *hdr = m->m_ops->get_real_data(m);

	return hdr->size;
}

/*
 * memblock_header_compact_get_size --
 *	(internal) returns the size stored in a compact header
 */
static size_t
memblock_header_compact_get_size(const struct memory_block *m)
{
	struct allocation_header_compact *hdr = m->m_ops->get_real_data(m);

	return hdr->size & ALLOC_HDR_FLAGS_MASK;
}

/*
 * memblock_header_none_get_size --
 *	(internal) determines the sizes of an object without a header
 */
static size_t
memblock_header_none_get_size(const struct memory_block *m)
{
	return m->m_ops->block_size(m);
}

/*
 * memblock_header_legacy_get_extra --
 *	(internal) returns the extra field stored in a legacy header
 */
static uint64_t
memblock_header_legacy_get_extra(const struct memory_block *m)
{
	struct allocation_header_legacy *hdr = m->m_ops->get_real_data(m);

	return hdr->type_num;
}

/*
 * memblock_header_compact_get_extra --
 *	(internal) returns the extra field stored in a compact header
 */
static uint64_t
memblock_header_compact_get_extra(const struct memory_block *m)
{
	struct allocation_header_compact *hdr = m->m_ops->get_real_data(m);

	return hdr->extra;
}

/*
 * memblock_header_none_get_extra --
 *	(internal) objects without a header don't have an extra field
 */
static uint64_t
memblock_header_none_get_extra(const struct memory_block *m)
{
	return 0;
}

/*
 * memblock_header_legacy_get_flags --
 *	(internal) returns the flags stored in a legacy header
 */
static uint16_t
memblock_header_legacy_get_flags(const struct memory_block *m)
{
	struct allocation_header_legacy *hdr = m->m_ops->get_real_data(m);

	return (uint16_t)(hdr->root_size >> ALLOC_HDR_SIZE_SHIFT);
}

/*
 * memblock_header_compact_get_flags --
 *	(internal) returns the flags stored in a compact header
 */
static uint16_t
memblock_header_compact_get_flags(const struct memory_block *m)
{
	struct allocation_header_compact *hdr = m->m_ops->get_real_data(m);

	return (uint16_t)(hdr->size >> ALLOC_HDR_SIZE_SHIFT);
}

/*
 * memblock_header_none_get_flags --
 *	(internal) objects without a header do not support flags
 */
static uint16_t
memblock_header_none_get_flags(const struct memory_block *m)
{
	return 0;
}

/*
 * memblock_header_legacy_write --
 *	(internal) writes a legacy header of an object
 */
static void
memblock_header_legacy_write(const struct memory_block *m,
	size_t size, uint64_t extra, uint16_t flags)
{
	struct allocation_header_legacy hdr;
	hdr.size = size;
	hdr.type_num = extra;
	hdr.root_size = ((uint64_t)flags << ALLOC_HDR_SIZE_SHIFT);

	struct allocation_header_legacy *hdrp = m->m_ops->get_real_data(m);

	VALGRIND_DO_MAKE_MEM_UNDEFINED(hdrp, sizeof(*hdrp));

	VALGRIND_ADD_TO_TX(hdrp, sizeof(*hdrp));
	pmemops_memcpy(&m->heap->p_ops, hdrp, &hdr,
		sizeof(hdr), /* legacy header is 64 bytes in size */
		PMEMOBJ_F_MEM_WC | PMEMOBJ_F_MEM_NODRAIN | PMEMOBJ_F_RELAXED);
	VALGRIND_REMOVE_FROM_TX(hdrp, sizeof(*hdrp));

	/* unused fields of the legacy headers are used as a red zone */
	VALGRIND_DO_MAKE_MEM_NOACCESS(hdrp->unused, sizeof(hdrp->unused));
}

/*
 * memblock_header_compact_write --
 *	(internal) writes a compact header of an object
 */
static void
memblock_header_compact_write(const struct memory_block *m,
	size_t size, uint64_t extra, uint16_t flags)
{
	COMPILE_ERROR_ON(ALLOC_HDR_COMPACT_SIZE > CACHELINE_SIZE);

	struct {
		struct allocation_header_compact hdr;
		uint8_t padding[CACHELINE_SIZE - ALLOC_HDR_COMPACT_SIZE];
	} padded;

	padded.hdr.size = size | ((uint64_t)flags << ALLOC_HDR_SIZE_SHIFT);
	padded.hdr.extra = extra;

	struct allocation_header_compact *hdrp = m->m_ops->get_real_data(m);

	VALGRIND_DO_MAKE_MEM_UNDEFINED(hdrp, sizeof(*hdrp));

	/*
	 * If possible write the entire header with a single memcpy, this allows
	 * the copy implementation to avoid a cache miss on a partial cache line
	 * write.
	 */
	size_t hdr_size = ALLOC_HDR_COMPACT_SIZE;
	if ((uintptr_t)hdrp % CACHELINE_SIZE == 0 && size >= sizeof(padded))
		hdr_size = sizeof(padded);

	VALGRIND_ADD_TO_TX(hdrp, hdr_size);

	pmemops_memcpy(&m->heap->p_ops, hdrp, &padded, hdr_size,
		PMEMOBJ_F_MEM_WC | PMEMOBJ_F_MEM_NODRAIN | PMEMOBJ_F_RELAXED);
	VALGRIND_DO_MAKE_MEM_UNDEFINED((char *)hdrp + ALLOC_HDR_COMPACT_SIZE,
		hdr_size - ALLOC_HDR_COMPACT_SIZE);

	VALGRIND_REMOVE_FROM_TX(hdrp, hdr_size);
}

/*
 * memblock_header_none_write --
 *	(internal) nothing to write
 */
static void
memblock_header_none_write(const struct memory_block *m,
	size_t size, uint64_t extra, uint16_t flags)
{
	/* NOP */
}

/*
 * memblock_header_legacy_invalidate --
 *	(internal) invalidates a legacy header
 */
static void
memblock_header_legacy_invalidate(const struct memory_block *m)
{
	struct allocation_header_legacy *hdr = m->m_ops->get_real_data(m);
	VALGRIND_SET_CLEAN(hdr, sizeof(*hdr));
}

/*
 * memblock_header_compact_invalidate --
 *	(internal) invalidates a compact header
 */
static void
memblock_header_compact_invalidate(const struct memory_block *m)
{
	struct allocation_header_compact *hdr = m->m_ops->get_real_data(m);
	VALGRIND_SET_CLEAN(hdr, sizeof(*hdr));
}

/*
 * memblock_no_header_invalidate --
 *	(internal) nothing to invalidate
 */
static void
memblock_header_none_invalidate(const struct memory_block *m)
{
	/* NOP */
}

/*
 * memblock_header_legacy_reinit --
 *	(internal) reinitializes a legacy header after a heap restart
 */
static void
memblock_header_legacy_reinit(const struct memory_block *m)
{
	struct allocation_header_legacy *hdr = m->m_ops->get_real_data(m);

	VALGRIND_DO_MAKE_MEM_DEFINED(hdr, sizeof(*hdr));

	/* unused fields of the legacy headers are used as a red zone */
	VALGRIND_DO_MAKE_MEM_NOACCESS(hdr->unused, sizeof(hdr->unused));
}

/*
 * memblock_header_compact_reinit --
 *	(internal) reinitializes a compact header after a heap restart
 */
static void
memblock_header_compact_reinit(const struct memory_block *m)
{
	struct allocation_header_compact *hdr = m->m_ops->get_real_data(m);

	VALGRIND_DO_MAKE_MEM_DEFINED(hdr, sizeof(*hdr));
}

/*
 * memblock_header_none_reinit --
 *	(internal) nothing to reinitialize
 */
static void
memblock_header_none_reinit(const struct memory_block *m)
{
	/* NOP */
}

static const struct {
	/* determines the sizes of an object */
	size_t (*get_size)(const struct memory_block *m);

	/* returns the extra field (if available, 0 if not) */
	uint64_t (*get_extra)(const struct memory_block *m);

	/* returns the flags stored in a header (if available, 0 if not) */
	uint16_t (*get_flags)(const struct memory_block *m);

	/*
	 * Stores size, extra info and flags in header of an object
	 * (if available, does nothing otherwise).
	 */
	void (*write)(const struct memory_block *m,
		size_t size, uint64_t extra, uint16_t flags);
	void (*invalidate)(const struct memory_block *m);

	/*
	 * Reinitializes a header after a heap restart (if available, does
	 * nothing otherwise) (VG).
	 */
	void (*reinit)(const struct memory_block *m);
} memblock_header_ops[MAX_HEADER_TYPES] = {
	[HEADER_LEGACY] = {
		memblock_header_legacy_get_size,
		memblock_header_legacy_get_extra,
		memblock_header_legacy_get_flags,
		memblock_header_legacy_write,
		memblock_header_legacy_invalidate,
		memblock_header_legacy_reinit,
	},
	[HEADER_COMPACT] = {
		memblock_header_compact_get_size,
		memblock_header_compact_get_extra,
		memblock_header_compact_get_flags,
		memblock_header_compact_write,
		memblock_header_compact_invalidate,
		memblock_header_compact_reinit,
	},
	[HEADER_NONE] = {
		memblock_header_none_get_size,
		memblock_header_none_get_extra,
		memblock_header_none_get_flags,
		memblock_header_none_write,
		memblock_header_none_invalidate,
		memblock_header_none_reinit,
	}
};

/*
 * memblock_run_default_nallocs -- returns the number of memory blocks
 *	available in the in a run with given parameters using the default
 *	fixed-bitmap algorithm
 */
static unsigned
memblock_run_default_nallocs(uint32_t *size_idx, uint16_t flags,
	uint64_t unit_size, uint64_t alignment)
{
	unsigned nallocs = (unsigned)
		(RUN_DEFAULT_SIZE_BYTES(*size_idx) / unit_size);

	while (nallocs > RUN_DEFAULT_BITMAP_NBITS) {
		LOG(3, "tried to create a run (%lu) with number "
			"of units (%u) exceeding the bitmap size (%u)",
			unit_size, nallocs, RUN_DEFAULT_BITMAP_NBITS);
		if (*size_idx > 1) {
			*size_idx -= 1;
			/* recalculate the number of allocations */
			nallocs = (uint32_t)
				(RUN_DEFAULT_SIZE_BYTES(*size_idx) / unit_size);
			LOG(3, "run (%lu) was constructed with "
				"fewer (%u) than requested chunks (%u)",
				unit_size, *size_idx, *size_idx + 1);
		} else {
			LOG(3, "run (%lu) was constructed with "
				"fewer units (%u) than optimal (%u), "
				"this might lead to "
				"inefficient memory utilization!",
				unit_size,
				RUN_DEFAULT_BITMAP_NBITS, nallocs);

			nallocs = RUN_DEFAULT_BITMAP_NBITS;
		}
	}

	return nallocs - (alignment ? 1 : 0);
}

/*
 * memblock_run_bitmap -- calculate bitmap parameters for given arguments
 */
void
memblock_run_bitmap(uint32_t *size_idx, uint16_t flags,
	uint64_t unit_size, uint64_t alignment, void *content,
	struct run_bitmap *b)
{
	ASSERTne(*size_idx, 0);

	/*
	 * Flexible bitmaps have a variably sized values array. The size varies
	 * depending on:
	 *	alignment - initial run alignment might require up-to a unit
	 *	size idx - the larger the run, the more units it carries
	 *	unit_size - the smaller the unit size, the more units per run
	 *
	 * The size of the bitmap also has to be calculated in such a way that
	 * the beginning of allocations data is cacheline aligned. This is
	 * required to perform many optimizations throughout the codebase.
	 * This alignment requirement means that some of the bitmap values might
	 * remain unused and will serve only as a padding for data.
	 */
	if (flags & CHUNK_FLAG_FLEX_BITMAP) {
		/*
		 * First calculate the number of values without accounting for
		 * the bitmap size.
		 */
		size_t content_size = RUN_CONTENT_SIZE_BYTES(*size_idx);
		b->nbits = (unsigned)(content_size / unit_size);
		b->nvalues = util_div_ceil(b->nbits, RUN_BITS_PER_VALUE);

		/*
		 * Then, align the number of values up, so that the cacheline
		 * alignment is preserved.
		 */
		b->nvalues = ALIGN_UP(b->nvalues + RUN_BASE_METADATA_VALUES, 8U)
			- RUN_BASE_METADATA_VALUES;

		/*
		 * This is the total number of bytes needed for the bitmap AND
		 * padding.
		 */
		b->size = b->nvalues * sizeof(*b->values);

		/*
		 * Calculate the number of allocations again, but this time
		 * accounting for the bitmap/padding.
		 */
		b->nbits = (unsigned)((content_size - b->size) / unit_size)
			- (alignment ? 1U : 0U);

		/*
		 * The last step is to calculate how much of the padding
		 * is left at the end of the bitmap.
		 */
		unsigned unused_bits = (b->nvalues * RUN_BITS_PER_VALUE)
			- b->nbits;
		unsigned unused_values = unused_bits / RUN_BITS_PER_VALUE;
		b->nvalues -= unused_values;

		b->values = (uint64_t *)content;

		return;
	}

	b->size = RUN_DEFAULT_BITMAP_SIZE;
	b->nbits = memblock_run_default_nallocs(size_idx, flags,
		unit_size, alignment);

	unsigned unused_bits = RUN_DEFAULT_BITMAP_NBITS - b->nbits;
	unsigned unused_values = unused_bits / RUN_BITS_PER_VALUE;
	b->nvalues = RUN_DEFAULT_BITMAP_VALUES - unused_values;

	b->values = (uint64_t *)content;
}

/*
 * run_get_bitmap -- initializes run bitmap information
 */
static void
run_get_bitmap(const struct memory_block *m, struct run_bitmap *b)
{
	struct chunk_header *hdr = heap_get_chunk_hdr(m->heap, m);
	struct chunk_run *run = heap_get_chunk_run(m->heap, m);

	uint32_t size_idx = hdr->size_idx;
	memblock_run_bitmap(&size_idx, hdr->flags, run->hdr.block_size,
		run->hdr.alignment, run->content, b);
	ASSERTeq(size_idx, hdr->size_idx);
}

/*
 * huge_block_size -- returns the compile-time constant which defines the
 *	huge memory block size.
 */
static size_t
huge_block_size(const struct memory_block *m)
{
	return CHUNKSIZE;
}

/*
 * run_block_size -- looks for the right chunk and returns the block size
 *	information that is attached to the run block metadata.
 */
static size_t
run_block_size(const struct memory_block *m)
{
	struct chunk_run *run = heap_get_chunk_run(m->heap, m);

	return run->hdr.block_size;
}

/*
 * huge_get_real_data -- returns pointer to the beginning data of a huge block
 */
static void *
huge_get_real_data(const struct memory_block *m)
{
	return heap_get_chunk(m->heap, m)->data;
}

/*
 * run_get_data_start -- (internal) returns the pointer to the beginning of
 *	allocations in a run
 */
static char *
run_get_data_start(const struct memory_block *m)
{
	struct chunk_header *hdr = heap_get_chunk_hdr(m->heap, m);
	struct chunk_run *run = heap_get_chunk_run(m->heap, m);

	struct run_bitmap b;
	run_get_bitmap(m, &b);

	if (hdr->flags & CHUNK_FLAG_ALIGNED) {
		/*
		 * Alignment is property of user data in allocations. And
		 * since objects have headers, we need to take them into
		 * account when calculating the address.
		 */
		uintptr_t hsize = header_type_to_size[m->header_type];
		uintptr_t base = (uintptr_t)run->content +
			b.size + hsize;
		return (char *)(ALIGN_UP(base, run->hdr.alignment) - hsize);
	} else {
		return (char *)&run->content + b.size;
	}
}

/*
 * run_get_data_offset -- (internal) returns the number of bytes between
 *	run base metadata and data
 */
static size_t
run_get_data_offset(const struct memory_block *m)
{
	struct chunk_run *run = heap_get_chunk_run(m->heap, m);
	return (size_t)run_get_data_start(m) - (size_t)&run->content;
}

/*
 * run_get_real_data -- returns pointer to the beginning data of a run block
 */
static void *
run_get_real_data(const struct memory_block *m)
{
	struct chunk_run *run = heap_get_chunk_run(m->heap, m);
	ASSERT(run->hdr.block_size != 0);

	return run_get_data_start(m) + (run->hdr.block_size * m->block_off);
}

/*
 * block_get_user_data -- returns pointer to the data of a block
 */
static void *
block_get_user_data(const struct memory_block *m)
{
	return (char *)m->m_ops->get_real_data(m) +
		header_type_to_size[m->header_type];
}

/*
 * chunk_get_chunk_hdr_value -- (internal) get value of a header for redo log
 */
static uint64_t
chunk_get_chunk_hdr_value(uint16_t type, uint16_t flags, uint32_t size_idx)
{
	uint64_t val;
	COMPILE_ERROR_ON(sizeof(struct chunk_header) != sizeof(uint64_t));

	struct chunk_header hdr;
	hdr.type = type;
	hdr.flags = flags;
	hdr.size_idx = size_idx;
	memcpy(&val, &hdr, sizeof(val));

	return val;
}

/*
 * huge_prep_operation_hdr -- prepares the new value of a chunk header that will
 *	be set after the operation concludes.
 */
static void
huge_prep_operation_hdr(const struct memory_block *m, enum memblock_state op,
	struct operation_context *ctx)
{
	struct chunk_header *hdr = heap_get_chunk_hdr(m->heap, m);

	/*
	 * Depending on the operation that needs to be performed a new chunk
	 * header needs to be prepared with the new chunk state.
	 */
	uint64_t val = chunk_get_chunk_hdr_value(
		op == MEMBLOCK_ALLOCATED ? CHUNK_TYPE_USED : CHUNK_TYPE_FREE,
		hdr->flags,
		m->size_idx);

	if (ctx == NULL) {
		util_atomic_store_explicit64((uint64_t *)hdr, val,
			memory_order_relaxed);
		pmemops_persist(&m->heap->p_ops, hdr, sizeof(*hdr));
	} else {
		operation_add_entry(ctx, hdr, val, ULOG_OPERATION_SET);
	}

	VALGRIND_DO_MAKE_MEM_NOACCESS(hdr + 1,
		(hdr->size_idx - 1) * sizeof(struct chunk_header));

	/*
	 * In the case of chunks larger than one unit the footer must be
	 * created immediately AFTER the persistent state is safely updated.
	 */
	if (m->size_idx == 1)
		return;

	struct chunk_header *footer = hdr + m->size_idx - 1;
	VALGRIND_DO_MAKE_MEM_UNDEFINED(footer, sizeof(*footer));

	val = chunk_get_chunk_hdr_value(CHUNK_TYPE_FOOTER, 0, m->size_idx);

	/*
	 * It's only safe to write the footer AFTER the persistent part of
	 * the operation have been successfully processed because the footer
	 * pointer might point to a currently valid persistent state
	 * of a different chunk.
	 * The footer entry change is updated as transient because it will
	 * be recreated at heap boot regardless - it's just needed for runtime
	 * operations.
	 */
	if (ctx == NULL) {
		util_atomic_store_explicit64((uint64_t *)footer, val,
			memory_order_relaxed);
		VALGRIND_SET_CLEAN(footer, sizeof(*footer));
	} else {
		operation_add_typed_entry(ctx,
			footer, val, ULOG_OPERATION_SET, LOG_TRANSIENT);
	}
}

/*
 * run_prep_operation_hdr -- prepares the new value for a select few bytes of
 *	a run bitmap that will be set after the operation concludes.
 *
 * It's VERY important to keep in mind that the particular value of the
 * bitmap this method is modifying must not be changed after this function
 * is called and before the operation is processed.
 */
static void
run_prep_operation_hdr(const struct memory_block *m, enum memblock_state op,
	struct operation_context *ctx)
{
	ASSERT(m->size_idx <= RUN_BITS_PER_VALUE);

	/*
	 * Free blocks are represented by clear bits and used blocks by set
	 * bits - which is the reverse of the commonly used scheme.
	 *
	 * Here a bit mask is prepared that flips the bits that represent the
	 * memory block provided by the caller - because both the size index and
	 * the block offset are tied 1:1 to the bitmap this operation is
	 * relatively simple.
	 */
	uint64_t bmask;
	if (m->size_idx == RUN_BITS_PER_VALUE) {
		ASSERTeq(m->block_off % RUN_BITS_PER_VALUE, 0);
		bmask = UINT64_MAX;
	} else {
		bmask = ((1ULL << m->size_idx) - 1ULL) <<
				(m->block_off % RUN_BITS_PER_VALUE);
	}

	/*
	 * The run bitmap is composed of several 8 byte values, so a proper
	 * element of the bitmap array must be selected.
	 */
	unsigned bpos = m->block_off / RUN_BITS_PER_VALUE;

	struct run_bitmap b;
	run_get_bitmap(m, &b);

	/* the bit mask is applied immediately by the add entry operations */
	if (op == MEMBLOCK_ALLOCATED) {
		operation_add_entry(ctx, &b.values[bpos],
			bmask, ULOG_OPERATION_OR);
	} else if (op == MEMBLOCK_FREE) {
		operation_add_entry(ctx, &b.values[bpos],
			~bmask, ULOG_OPERATION_AND);
	} else {
		ASSERT(0);
	}
}

/*
 * huge_get_lock -- because huge memory blocks are always allocated from a
 *	single bucket there's no reason to lock them - the bucket itself is
 *	protected.
 */
static os_mutex_t *
huge_get_lock(const struct memory_block *m)
{
	return NULL;
}

/*
 * run_get_lock -- gets the runtime mutex from the heap.
 */
static os_mutex_t *
run_get_lock(const struct memory_block *m)
{
	return heap_get_run_lock(m->heap, m->chunk_id);
}

/*
 * huge_get_state -- returns whether a huge block is allocated or not
 */
static enum memblock_state
huge_get_state(const struct memory_block *m)
{
	struct chunk_header *hdr = heap_get_chunk_hdr(m->heap, m);

	if (hdr->type == CHUNK_TYPE_USED)
		return MEMBLOCK_ALLOCATED;

	if (hdr->type == CHUNK_TYPE_FREE)
		return MEMBLOCK_FREE;

	return MEMBLOCK_STATE_UNKNOWN;
}

/*
 * huge_get_state -- returns whether a block from a run is allocated or not
 */
static enum memblock_state
run_get_state(const struct memory_block *m)
{
	struct run_bitmap b;
	run_get_bitmap(m, &b);

	unsigned v = m->block_off / RUN_BITS_PER_VALUE;
	uint64_t bitmap = b.values[v];
	unsigned bit = m->block_off % RUN_BITS_PER_VALUE;

	unsigned bit_last = bit + m->size_idx;
	ASSERT(bit_last <= RUN_BITS_PER_VALUE);

	for (unsigned i = bit; i < bit_last; ++i) {
		if (!BIT_IS_CLR(bitmap, i)) {
			return MEMBLOCK_ALLOCATED;
		}
	}

	return MEMBLOCK_FREE;
}

/*
 * huge_ensure_header_type -- checks the header type of a chunk and modifies
 *	it if necessary. This is fail-safe atomic.
 */
static void
huge_ensure_header_type(const struct memory_block *m,
	enum header_type t)
{
	struct chunk_header *hdr = heap_get_chunk_hdr(m->heap, m);
	ASSERTeq(hdr->type, CHUNK_TYPE_FREE);

	if ((hdr->flags & header_type_to_flag[t]) == 0) {
		VALGRIND_ADD_TO_TX(hdr, sizeof(*hdr));
		uint16_t f = ((uint16_t)header_type_to_flag[t]);
		hdr->flags |= f;
		pmemops_persist(&m->heap->p_ops, hdr, sizeof(*hdr));
		VALGRIND_REMOVE_FROM_TX(hdr, sizeof(*hdr));
	}
}

/*
 * run_ensure_header_type -- runs must be created with appropriate header type.
 */
static void
run_ensure_header_type(const struct memory_block *m,
	enum header_type t)
{
#ifdef DEBUG
	struct chunk_header *hdr = heap_get_chunk_hdr(m->heap, m);
	ASSERTeq(hdr->type, CHUNK_TYPE_RUN);
	ASSERT((hdr->flags & header_type_to_flag[t]) == header_type_to_flag[t]);
#endif
}

/*
 * block_get_real_size -- returns the size of a memory block that includes all
 *	of the overhead (headers)
 */
static size_t
block_get_real_size(const struct memory_block *m)
{
	/*
	 * There are two valid ways to get a size. If the memory block
	 * initialized properly and the size index is set, the chunk unit size
	 * can be simply multiplied by that index, otherwise we need to look at
	 * the allocation header.
	 */
	if (m->size_idx != 0) {
		return m->m_ops->block_size(m) * m->size_idx;
	} else {
		return memblock_header_ops[m->header_type].get_size(m);
	}
}

/*
 * block_get_user_size -- returns the size of a memory block without overheads,
 *	this is the size of a data block that can be used.
 */
static size_t
block_get_user_size(const struct memory_block *m)
{
	return block_get_real_size(m) - header_type_to_size[m->header_type];
}

/*
 * block_write_header -- writes a header of an allocation
 */
static void
block_write_header(const struct memory_block *m,
	uint64_t extra_field, uint16_t flags)
{
	memblock_header_ops[m->header_type].write(m,
		block_get_real_size(m), extra_field, flags);
}

/*
 * block_invalidate -- invalidates allocation data and header
 */
static void
block_invalidate(const struct memory_block *m)
{
	void *data = m->m_ops->get_user_data(m);
	size_t size = m->m_ops->get_user_size(m);
	VALGRIND_SET_CLEAN(data, size);

	memblock_header_ops[m->header_type].invalidate(m);
}

/*
 * block_reinit_header -- reinitializes a block after a heap restart
 */
static void
block_reinit_header(const struct memory_block *m)
{
	memblock_header_ops[m->header_type].reinit(m);
}

/*
 * block_get_extra -- returns the extra field of an allocation
 */
static uint64_t
block_get_extra(const struct memory_block *m)
{
	return memblock_header_ops[m->header_type].get_extra(m);
}

/*
 * block_get_flags -- returns the flags of an allocation
 */
static uint16_t
block_get_flags(const struct memory_block *m)
{
	return memblock_header_ops[m->header_type].get_flags(m);
}

/*
 * heap_run_process_bitmap_value -- (internal) looks for unset bits in the
 * value, creates a valid memory block out of them and inserts that
 * block into the given bucket.
 */
static int
run_process_bitmap_value(const struct memory_block *m,
	uint64_t value, uint32_t base_offset, object_callback cb, void *arg)
{
	int ret = 0;

	uint64_t shift = 0; /* already processed bits */
	struct memory_block s = *m;
	do {
		/*
		 * Shift the value so that the next memory block starts on the
		 * least significant position:
		 *	..............0 (free block)
		 * or	..............1 (used block)
		 */
		uint64_t shifted = value >> shift;

		/* all clear or set bits indicate the end of traversal */
		if (shifted == 0) {
			/*
			 * Insert the remaining blocks as free. Remember that
			 * unsigned values are always zero-filled, so we must
			 * take the current shift into account.
			 */
			s.block_off = (uint32_t)(base_offset + shift);
			s.size_idx = (uint32_t)(RUN_BITS_PER_VALUE - shift);

			if ((ret = cb(&s, arg)) != 0)
				return ret;

			break;
		} else if (shifted == UINT64_MAX) {
			break;
		}

		/*
		 * Offset and size of the next free block, either of these
		 * can be zero depending on where the free block is located
		 * in the value.
		 */
		unsigned off = (unsigned)util_lssb_index64(~shifted);
		unsigned size = (unsigned)util_lssb_index64(shifted);

		shift += off + size;

		if (size != 0) { /* zero size means skip to the next value */
			s.block_off = (uint32_t)(base_offset + (shift - size));
			s.size_idx = (uint32_t)(size);

			memblock_rebuild_state(m->heap, &s);
			if ((ret = cb(&s, arg)) != 0)
				return ret;
		}
	} while (shift != RUN_BITS_PER_VALUE);

	return 0;
}

/*
 * run_iterate_free -- iterates over free blocks in a run
 */
static int
run_iterate_free(const struct memory_block *m, object_callback cb, void *arg)
{
	int ret = 0;
	uint32_t block_off = 0;

	struct run_bitmap b;
	run_get_bitmap(m, &b);

	struct memory_block nm = *m;
	for (unsigned i = 0; i < b.nvalues; ++i) {
		uint64_t v = b.values[i];
		ASSERT((uint64_t)RUN_BITS_PER_VALUE * (uint64_t)i
			<= UINT32_MAX);
		block_off = RUN_BITS_PER_VALUE * i;
		ret = run_process_bitmap_value(&nm, v, block_off, cb, arg);
		if (ret != 0)
			return ret;
	}

	return 0;
}

/*
 * run_iterate_used -- iterates over used blocks in a run
 */
static int
run_iterate_used(const struct memory_block *m, object_callback cb, void *arg)
{
	uint32_t i = m->block_off / RUN_BITS_PER_VALUE;
	uint32_t block_start = m->block_off % RUN_BITS_PER_VALUE;
	uint32_t block_off;

	struct chunk_run *run = heap_get_chunk_run(m->heap, m);

	struct memory_block iter = *m;

	struct run_bitmap b;
	run_get_bitmap(m, &b);

	for (; i < b.nvalues; ++i) {
		uint64_t v = b.values[i];
		block_off = (uint32_t)(RUN_BITS_PER_VALUE * i);

		for (uint32_t j = block_start; j < RUN_BITS_PER_VALUE; ) {
			if (block_off + j >= (uint32_t)b.nbits)
				break;

			if (!BIT_IS_CLR(v, j)) {
				iter.block_off = (uint32_t)(block_off + j);

				/*
				 * The size index of this memory block cannot be
				 * retrieved at this time because the header
				 * might not be initialized in valgrind yet.
				 */
				iter.size_idx = 0;

				if (cb(&iter, arg) != 0)
					return 1;

				iter.size_idx = CALC_SIZE_IDX(
					run->hdr.block_size,
					iter.m_ops->get_real_size(&iter));
				j = (uint32_t)(j + iter.size_idx);
			} else {
				++j;
			}
		}
		block_start = 0;
	}

	return 0;
}

/*
 * huge_iterate_free -- calls cb on memory block if it's free
 */
static int
huge_iterate_free(const struct memory_block *m, object_callback cb, void *arg)
{
	struct chunk_header *hdr = heap_get_chunk_hdr(m->heap, m);

	return hdr->type == CHUNK_TYPE_FREE ? cb(m, arg) : 0;
}

/*
 * huge_iterate_free -- calls cb on memory block if it's used
 */
static int
huge_iterate_used(const struct memory_block *m, object_callback cb, void *arg)
{
	struct chunk_header *hdr = heap_get_chunk_hdr(m->heap, m);

	return hdr->type == CHUNK_TYPE_USED ? cb(m, arg) : 0;
}

/*
 * huge_vg_init -- initializes chunk metadata in memcheck state
 */
static void
huge_vg_init(const struct memory_block *m, int objects,
	object_callback cb, void *arg)
{
	struct zone *z = ZID_TO_ZONE(m->heap->layout, m->zone_id);
	struct chunk_header *hdr = heap_get_chunk_hdr(m->heap, m);
	struct chunk *chunk = heap_get_chunk(m->heap, m);
	VALGRIND_DO_MAKE_MEM_DEFINED(hdr, sizeof(*hdr));

	/*
	 * Mark unused chunk headers as not accessible.
	 */
	VALGRIND_DO_MAKE_MEM_NOACCESS(
		&z->chunk_headers[m->chunk_id + 1],
		(m->size_idx - 1) *
		sizeof(struct chunk_header));

	size_t size = block_get_real_size(m);
	VALGRIND_DO_MAKE_MEM_NOACCESS(chunk, size);

	if (objects && huge_get_state(m) == MEMBLOCK_ALLOCATED) {
		if (cb(m, arg) != 0)
			FATAL("failed to initialize valgrind state");
	}
}

/*
 * run_vg_init -- initializes run metadata in memcheck state
 */
static void
run_vg_init(const struct memory_block *m, int objects,
	object_callback cb, void *arg)
{
	struct zone *z = ZID_TO_ZONE(m->heap->layout, m->zone_id);
	struct chunk_header *hdr = heap_get_chunk_hdr(m->heap, m);
	struct chunk_run *run = heap_get_chunk_run(m->heap, m);
	VALGRIND_DO_MAKE_MEM_DEFINED(hdr, sizeof(*hdr));

	/* set the run metadata as defined */
	VALGRIND_DO_MAKE_MEM_DEFINED(run, RUN_BASE_METADATA_SIZE);

	struct run_bitmap b;
	run_get_bitmap(m, &b);

	/*
	 * Mark run data headers as defined.
	 */
	for (unsigned j = 1; j < m->size_idx; ++j) {
		struct chunk_header *data_hdr =
			&z->chunk_headers[m->chunk_id + j];
		VALGRIND_DO_MAKE_MEM_DEFINED(data_hdr,
			sizeof(struct chunk_header));
		ASSERTeq(data_hdr->type, CHUNK_TYPE_RUN_DATA);
	}

	VALGRIND_DO_MAKE_MEM_NOACCESS(run, SIZEOF_RUN(run, m->size_idx));

	/* set the run bitmap as defined */
	VALGRIND_DO_MAKE_MEM_DEFINED(run, b.size + RUN_BASE_METADATA_SIZE);

	if (objects) {
		if (run_iterate_used(m, cb, arg) != 0)
			FATAL("failed to initialize valgrind state");
	}
}

/*
 * run_reinit_chunk -- run reinitialization on first zone traversal
 */
static void
run_reinit_chunk(const struct memory_block *m)
{
	/* noop */
}

/*
 * huge_write_footer -- (internal) writes a chunk footer
 */
static void
huge_write_footer(struct chunk_header *hdr, uint32_t size_idx)
{
	if (size_idx == 1) /* that would overwrite the header */
		return;

	VALGRIND_DO_MAKE_MEM_UNDEFINED(hdr + size_idx - 1, sizeof(*hdr));

	struct chunk_header f = *hdr;
	f.type = CHUNK_TYPE_FOOTER;
	f.size_idx = size_idx;
	*(hdr + size_idx - 1) = f;
	/* no need to persist, footers are recreated in heap_populate_buckets */
	VALGRIND_SET_CLEAN(hdr + size_idx - 1, sizeof(f));
}

/*
 * huge_reinit_chunk -- chunk reinitialization on first zone traversal
 */
static void
huge_reinit_chunk(const struct memory_block *m)
{
	struct chunk_header *hdr = heap_get_chunk_hdr(m->heap, m);
	if (hdr->type == CHUNK_TYPE_USED)
		huge_write_footer(hdr, hdr->size_idx);
}

/*
 * run_calc_free -- calculates the number of free units in a run
 */
static void
run_calc_free(const struct memory_block *m,
	uint32_t *free_space, uint32_t *max_free_block)
{
	struct run_bitmap b;
	run_get_bitmap(m, &b);
	for (unsigned i = 0; i < b.nvalues; ++i) {
		uint64_t value = ~b.values[i];
		if (value == 0)
			continue;

		uint32_t free_in_value = util_popcount64(value);
		*free_space = *free_space + free_in_value;

		/*
		 * If this value has less free blocks than already found max,
		 * there's no point in calculating.
		 */
		if (free_in_value < *max_free_block)
			continue;

		/* if the entire value is empty, no point in calculating */
		if (free_in_value == RUN_BITS_PER_VALUE) {
			*max_free_block = RUN_BITS_PER_VALUE;
			continue;
		}

		/* if already at max, no point in calculating */
		if (*max_free_block == RUN_BITS_PER_VALUE)
			continue;

		/*
		 * Calculate the biggest free block in the bitmap.
		 * This algorithm is not the most clever imaginable, but it's
		 * easy to implement and fast enough.
		 */
		uint16_t n = 0;
		while (value != 0) {
			value &= (value << 1ULL);
			n++;
		}

		if (n > *max_free_block)
			*max_free_block = n;
	}
}

static const struct memory_block_ops mb_ops[MAX_MEMORY_BLOCK] = {
	[MEMORY_BLOCK_HUGE] = {
		.block_size = huge_block_size,
		.prep_hdr = huge_prep_operation_hdr,
		.get_lock = huge_get_lock,
		.get_state = huge_get_state,
		.get_user_data = block_get_user_data,
		.get_real_data = huge_get_real_data,
		.get_user_size = block_get_user_size,
		.get_real_size = block_get_real_size,
		.write_header = block_write_header,
		.invalidate = block_invalidate,
		.ensure_header_type = huge_ensure_header_type,
		.reinit_header = block_reinit_header,
		.vg_init = huge_vg_init,
		.get_extra = block_get_extra,
		.get_flags = block_get_flags,
		.iterate_free = huge_iterate_free,
		.iterate_used = huge_iterate_used,
		.reinit_chunk = huge_reinit_chunk,
		.calc_free = NULL,
		.get_bitmap = NULL,
	},
	[MEMORY_BLOCK_RUN] = {
		.block_size = run_block_size,
		.prep_hdr = run_prep_operation_hdr,
		.get_lock = run_get_lock,
		.get_state = run_get_state,
		.get_user_data = block_get_user_data,
		.get_real_data = run_get_real_data,
		.get_user_size = block_get_user_size,
		.get_real_size = block_get_real_size,
		.write_header = block_write_header,
		.invalidate = block_invalidate,
		.ensure_header_type = run_ensure_header_type,
		.reinit_header = block_reinit_header,
		.vg_init = run_vg_init,
		.get_extra = block_get_extra,
		.get_flags = block_get_flags,
		.iterate_free = run_iterate_free,
		.iterate_used = run_iterate_used,
		.reinit_chunk = run_reinit_chunk,
		.calc_free = run_calc_free,
		.get_bitmap = run_get_bitmap,
	}
};

/*
 * memblock_huge_init -- initializes a new huge memory block
 */
struct memory_block
memblock_huge_init(struct palloc_heap *heap,
	uint32_t chunk_id, uint32_t zone_id, uint32_t size_idx)
{
	struct memory_block m = MEMORY_BLOCK_NONE;
	m.chunk_id = chunk_id;
	m.zone_id = zone_id;
	m.size_idx = size_idx;
	m.heap = heap;

	struct chunk_header nhdr = {
		.type = CHUNK_TYPE_FREE,
		.flags = 0,
		.size_idx = size_idx
	};

	struct chunk_header *hdr = heap_get_chunk_hdr(heap, &m);

	VALGRIND_DO_MAKE_MEM_UNDEFINED(hdr, sizeof(*hdr));
	VALGRIND_ANNOTATE_NEW_MEMORY(hdr, sizeof(*hdr));

	*hdr = nhdr; /* write the entire header (8 bytes) at once */

	pmemops_persist(&heap->p_ops, hdr, sizeof(*hdr));

	huge_write_footer(hdr, size_idx);

	memblock_rebuild_state(heap, &m);

	return m;
}

/*
 * memblock_run_init -- initializes a new run memory block
 */
struct memory_block
memblock_run_init(struct palloc_heap *heap,
	uint32_t chunk_id, uint32_t zone_id, uint32_t size_idx, uint16_t flags,
	uint64_t unit_size, uint64_t alignment)
{
	ASSERTne(size_idx, 0);

	struct memory_block m = MEMORY_BLOCK_NONE;
	m.chunk_id = chunk_id;
	m.zone_id = zone_id;
	m.size_idx = size_idx;
	m.heap = heap;

	struct zone *z = ZID_TO_ZONE(heap->layout, zone_id);

	struct chunk_run *run = heap_get_chunk_run(heap, &m);
	size_t runsize = SIZEOF_RUN(run, size_idx);

	VALGRIND_DO_MAKE_MEM_UNDEFINED(run, runsize);

	/* add/remove chunk_run and chunk_header to valgrind transaction */
	VALGRIND_ADD_TO_TX(run, runsize);
	run->hdr.block_size = unit_size;
	run->hdr.alignment = alignment;

	struct run_bitmap b;
	memblock_run_bitmap(&size_idx, flags, unit_size, alignment,
		run->content, &b);

	size_t bitmap_size = b.size;

	/* set all the bits */
	memset(b.values, 0xFF, bitmap_size);

	/* clear only the bits available for allocations from this bucket */
	memset(b.values, 0, sizeof(*b.values) * (b.nvalues - 1));

	unsigned trailing_bits = b.nbits % RUN_BITS_PER_VALUE;
	uint64_t last_value = UINT64_MAX << trailing_bits;
	b.values[b.nvalues - 1] = last_value;

	VALGRIND_REMOVE_FROM_TX(run, runsize);

	pmemops_flush(&heap->p_ops, run,
		sizeof(struct chunk_run_header) +
		bitmap_size);

	struct chunk_header run_data_hdr;
	run_data_hdr.type = CHUNK_TYPE_RUN_DATA;
	run_data_hdr.flags = 0;

	VALGRIND_ADD_TO_TX(&z->chunk_headers[chunk_id],
		sizeof(struct chunk_header) * size_idx);

	struct chunk_header *data_hdr;
	for (unsigned i = 1; i < size_idx; ++i) {
		data_hdr = &z->chunk_headers[chunk_id + i];
		VALGRIND_DO_MAKE_MEM_UNDEFINED(data_hdr, sizeof(*data_hdr));
		VALGRIND_ANNOTATE_NEW_MEMORY(data_hdr, sizeof(*data_hdr));
		run_data_hdr.size_idx = i;
		*data_hdr = run_data_hdr;
	}
	pmemops_persist(&heap->p_ops,
		&z->chunk_headers[chunk_id + 1],
		sizeof(struct chunk_header) * (size_idx - 1));

	struct chunk_header *hdr = &z->chunk_headers[chunk_id];
	ASSERT(hdr->type == CHUNK_TYPE_FREE);

	VALGRIND_ANNOTATE_NEW_MEMORY(hdr, sizeof(*hdr));

	struct chunk_header run_hdr;
	run_hdr.size_idx = hdr->size_idx;
	run_hdr.type = CHUNK_TYPE_RUN;
	run_hdr.flags = flags;
	*hdr = run_hdr;
	pmemops_persist(&heap->p_ops, hdr, sizeof(*hdr));

	VALGRIND_REMOVE_FROM_TX(&z->chunk_headers[chunk_id],
		sizeof(struct chunk_header) * size_idx);

	memblock_rebuild_state(heap, &m);

	return m;
}

/*
 * memblock_detect_type -- looks for the corresponding chunk header and
 *	depending on the chunks type returns the right memory block type
 */
static enum memory_block_type
memblock_detect_type(struct palloc_heap *heap, const struct memory_block *m)
{
	enum memory_block_type ret;

	switch (heap_get_chunk_hdr(heap, m)->type) {
		case CHUNK_TYPE_RUN:
		case CHUNK_TYPE_RUN_DATA:
			ret = MEMORY_BLOCK_RUN;
			break;
		case CHUNK_TYPE_FREE:
		case CHUNK_TYPE_USED:
		case CHUNK_TYPE_FOOTER:
			ret = MEMORY_BLOCK_HUGE;
			break;
		default:
			/* unreachable */
			FATAL("possible zone chunks metadata corruption");
	}
	return ret;
}

/*
 * memblock_from_offset -- resolves a memory block data from an offset that
 *	originates from the heap
 */
struct memory_block
memblock_from_offset_opt(struct palloc_heap *heap, uint64_t off, int size)
{
	struct memory_block m = MEMORY_BLOCK_NONE;
	m.heap = heap;

	off -= HEAP_PTR_TO_OFF(heap, &heap->layout->zone0);
	m.zone_id = (uint32_t)(off / ZONE_MAX_SIZE);

	off -= (ZONE_MAX_SIZE * m.zone_id) + sizeof(struct zone);
	m.chunk_id = (uint32_t)(off / CHUNKSIZE);

	struct chunk_header *hdr = heap_get_chunk_hdr(heap, &m);

	if (hdr->type == CHUNK_TYPE_RUN_DATA)
		m.chunk_id -= hdr->size_idx;

	off -= CHUNKSIZE * m.chunk_id;

	m.header_type = memblock_header_type(&m);

	off -= header_type_to_size[m.header_type];

	m.type = off != 0 ? MEMORY_BLOCK_RUN : MEMORY_BLOCK_HUGE;
	ASSERTeq(memblock_detect_type(heap, &m), m.type);

	m.m_ops = &mb_ops[m.type];

	uint64_t unit_size = m.m_ops->block_size(&m);

	if (off != 0) { /* run */
		off -= run_get_data_offset(&m);
		off -= RUN_BASE_METADATA_SIZE;
		m.block_off = (uint16_t)(off / unit_size);
		off -= m.block_off * unit_size;
	}

	m.size_idx = !size ? 0 : CALC_SIZE_IDX(unit_size,
		memblock_header_ops[m.header_type].get_size(&m));

	ASSERTeq(off, 0);

	return m;
}

/*
 * memblock_from_offset -- returns memory block with size
 */
struct memory_block
memblock_from_offset(struct palloc_heap *heap, uint64_t off)
{
	return memblock_from_offset_opt(heap, off, 1);
}

/*
 * memblock_rebuild_state -- fills in the runtime-state related fields of a
 *	memory block structure
 *
 * This function must be called on all memory blocks that were created by hand
 * (as opposed to retrieved from memblock_from_offset function).
 */
void
memblock_rebuild_state(struct palloc_heap *heap, struct memory_block *m)
{
	m->heap = heap;
	m->header_type = memblock_header_type(m);
	m->type = memblock_detect_type(heap, m);
	m->m_ops = &mb_ops[m->type];
}
