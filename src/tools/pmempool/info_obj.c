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
 * info_obj.c -- pmempool info command source file for obj pool
 */
#include <stdlib.h>
#include <stdbool.h>
#include <err.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <assert.h>

#include "common.h"
#include "output.h"
#include "info.h"

#define BITMAP_BUFF_SIZE 1024

#define OFF_TO_PTR(pop, off) ((void *)((uintptr_t)(pop) + (off)))

#define PTR_TO_OFF(pop, ptr) ((uintptr_t)ptr - (uintptr_t)pop)

#define DEFAULT_BUCKET MAX_BUCKETS

typedef void (*pvector_callback_fn)(struct pmem_info *pip, int v, int vnum,
		void *ptr, size_t i);

/*
 * lane_need_recovery_redo -- return 1 if redo log needs recovery
 */
static int
lane_need_recovery_redo(struct redo_log *redo, size_t nentries)
{
	/* Needs recovery if any of redo log entries has finish flag set */
	for (size_t i = 0; i < nentries; i++) {
		if (redo[i].offset & REDO_FINISH_FLAG)
			return 1;
	}

	return 0;
}

/*
 * lane_need_recovery_list -- return 1 if list's section needs recovery
 */
static int
lane_need_recovery_list(struct pmem_info *pip,
	struct lane_section_layout *layout)
{
	struct lane_list_layout *section = (struct lane_list_layout *)layout;

	/*
	 * The list section needs recovery if redo log needs recovery or
	 * object's offset or size are nonzero.
	 */
	return lane_need_recovery_redo(&section->redo[0], REDO_NUM_ENTRIES) ||
		section->obj_offset;
}

/*
 * lane_need_recovery_alloc -- return 1 if allocator's section needs recovery
 */
static int
lane_need_recovery_alloc(struct pmem_info *pip,
	struct lane_section_layout *layout)
{
	struct lane_alloc_layout *section =
		(struct lane_alloc_layout *)layout;

	/* there is just a redo log */
	return lane_need_recovery_redo(&section->redo[0], ALLOC_REDO_LOG_SIZE);
}

#define PVECTOR_EMPTY(_pvec) ((_pvec).embedded[0] == 0)

/*
 * lane_need_recovery_tx -- return 1 if transaction's section needs recovery
 */
static int
lane_need_recovery_tx(struct pmem_info *pip,
	struct lane_section_layout *layout)
{
	struct lane_tx_layout *section = (struct lane_tx_layout *)layout;

	int set_cache = 0;

	uint64_t off = section->undo_log[UNDO_SET_CACHE].embedded[0];

	if (off != 0) {
		struct tx_range_cache *cache = OFF_TO_PTR(pip->obj.pop, off);
		struct tx_range *range = (struct tx_range *)&cache->range[0];

		set_cache = (range->offset && range->size);
	}

	/*
	 * The transaction section needs recovery
	 * if state is not committed and
	 * any undo log not empty
	 */
	return section->state == TX_STATE_NONE &&
		(!PVECTOR_EMPTY(section->undo_log[UNDO_ALLOC]) ||
		!PVECTOR_EMPTY(section->undo_log[UNDO_FREE]) ||
		!PVECTOR_EMPTY(section->undo_log[UNDO_SET]) ||
		set_cache);
}

/*
 * lane_need_recovery -- return 1 if lane section needs recovery
 */
static int
lane_need_recovery(struct pmem_info *pip, struct lane_layout *lane)
{
	int alloc = lane_need_recovery_alloc(pip,
			&lane->sections[LANE_SECTION_ALLOCATOR]);
	int list = lane_need_recovery_list(pip,
			&lane->sections[LANE_SECTION_LIST]);
	int tx = lane_need_recovery_tx(pip,
			&lane->sections[LANE_SECTION_TRANSACTION]);

	return alloc || list || tx;
}

/*
 * heap_size_to_class -- get index of class of given allocation size
 */
static int
heap_size_to_class(size_t size)
{
	if (!size)
		return -1;

	if (size == CHUNKSIZE)
		return DEFAULT_BUCKET;

	return (int)SIZE_TO_ALLOC_BLOCKS(size);
}

/*
 * heal_class_to_size -- get size of allocation class
 */
static uint64_t
heap_class_to_size(int class)
{
	if (class == DEFAULT_BUCKET)
		return CHUNKSIZE;

	return (uint64_t)(class * ALLOC_BLOCK_SIZE);
}

/*
 * get_bitmap_size -- get number of used bits in chunk run's bitmap
 */
static uint32_t
get_bitmap_size(struct chunk_run *run)
{
	uint64_t size = RUNSIZE / run->block_size;
	assert(size <= UINT32_MAX);
	return (uint32_t)size;
}

/*
 * get_bitmap_reserved -- get number of reserved blocks in chunk run
 */
static int
get_bitmap_reserved(struct chunk_run *run, uint32_t *reserved)
{
	uint64_t nvals = 0;
	uint64_t last_val = 0;
	if (util_heap_get_bitmap_params(run->block_size, NULL, &nvals,
			&last_val))
		return -1;

	uint32_t ret = 0;
	for (uint64_t i = 0; i < nvals - 1; i++)
		ret += util_count_ones(run->bitmap[i]);
	ret += util_count_ones(run->bitmap[nvals - 1] & ~last_val);

	*reserved = ret;

	return 0;
}

/*
 * get_bitmap_str -- get bitmap single value string
 */
static const char *
get_bitmap_str(uint64_t val, unsigned values)
{
	static char buff[BITMAP_BUFF_SIZE];

	unsigned j = 0;
	for (unsigned i = 0; i < values && j < BITMAP_BUFF_SIZE - 3; i++) {
		buff[j++] = ((val & ((uint64_t)1 << i)) ? 'x' : '.');
		if ((i + 1) % RUN_UNIT_MAX == 0)
			buff[j++] = ' ';
	}

	buff[j] = '\0';

	return buff;
}

/*
 * pmem_obj_stats_get_type -- get stats for specified type number
 */
static struct pmem_obj_type_stats *
pmem_obj_stats_get_type(struct pmem_obj_stats *stats, uint64_t type_num)
{
	struct pmem_obj_type_stats *type;
	struct pmem_obj_type_stats *type_dest = NULL;
	TAILQ_FOREACH(type, &stats->type_stats, next) {
		if (type->type_num == type_num)
			return type;

		if (!type_dest && type->type_num > type_num)
			type_dest = type;
	}

	type = calloc(1, sizeof(*type));
	if (!type) {
		outv_err("cannot allocate memory for type stats\n");
		exit(EXIT_FAILURE);
	}

	type->type_num = type_num;
	if (type_dest)
		TAILQ_INSERT_BEFORE(type_dest, type, next);
	else
		TAILQ_INSERT_TAIL(&stats->type_stats, type, next);

	return type;
}

/*
 * info_obj_redo -- print redo log entries
 */
static void
info_obj_redo(int v, struct redo_log *redo, size_t nentries)
{
	outv_field(v, "Redo log entries", "%lu", nentries);
	for (size_t i = 0; i < nentries; i++) {
		outv(v, "%010u: "
			"Offset: 0x%016jx "
			"Value: 0x%016jx "
			"Finish flag: %d\n",
			i,
			redo[i].offset & REDO_FLAG_MASK,
			redo[i].value,
			redo[i].offset & REDO_FINISH_FLAG);
	}
}

/*
 * info_obj_lane_alloc -- print allocator's lane section
 */
static void
info_obj_lane_alloc(int v, struct lane_section_layout *layout)
{
	struct lane_alloc_layout *section =
		(struct lane_alloc_layout *)layout;
	info_obj_redo(v, &section->redo[0], ALLOC_REDO_LOG_SIZE);
}

/*
 * info_obj_lane_list -- print list's lane section
 */
static void
info_obj_lane_list(struct pmem_info *pip, int v,
	struct lane_section_layout *layout)
{
	struct lane_list_layout *section = (struct lane_list_layout *)layout;

	outv_field(v, "Object offset", "0x%016lx", section->obj_offset);
	info_obj_redo(v, &section->redo[0], REDO_NUM_ENTRIES);
}

static void
info_obj_pvector(struct pmem_info *pip, int vnum, int vobj,
	struct pvector *vec, const char *name, pvector_callback_fn cb)
{
	struct pvector_context *ctx = pvector_init(pip->obj.pop, vec);
	if (ctx == NULL) {
		outv_err("Cannot initialize pvector context\n");
		exit(EXIT_FAILURE);
	}

	size_t nvalues = pvector_nvalues(ctx);
	outv_field(vnum, name, "%lu element%s", nvalues,
			nvalues != 1 ? "s" : "");

	outv_indent(vobj, 1);

	size_t i = 0;
	uint64_t off;
	for (off = pvector_first(ctx); off != 0; off = pvector_next(ctx)) {
		cb(pip, vobj, vobj, OFF_TO_PTR(pip->obj.pop, off), i);
		i++;
	}

	pvector_delete(ctx);
	outv_indent(vobj, -1);
}

/*
 * info_obj_oob_hdr -- print OOB header
 */
static void
info_obj_oob_hdr(struct pmem_info *pip, int v, struct oob_header *oob)
{

	outv_title(v, "OOB Header");
	outv_hexdump(v && pip->args.vhdrdump, oob, sizeof(*oob),
		PTR_TO_OFF(pip->obj.pop, oob), 1);
	outv_field(v, "Undo offset", "%llx", oob->undo_entry_offset);
	outv_field(v, "Type Number", "0x%016lx", oob->type_num);

}

/*
 * info_obj_alloc_hdr -- print allocation header
 */
static void
info_obj_alloc_hdr(struct pmem_info *pip, int v,
	struct allocation_header *alloc)
{
	outv_title(v, "Allocation Header");
	outv_hexdump(v && pip->args.vhdrdump, alloc,
			sizeof(*alloc), PTR_TO_OFF(pip->obj.pop, alloc), 1);
	outv_field(v, "Zone id", "%u", alloc->zone_id);
	outv_field(v, "Chunk id", "%u", alloc->chunk_id);
	outv_field(v, "Size", "%s", out_get_size_str(alloc->size,
				pip->args.human));
}

/*
 * info_obj_object_hdr -- print object headers and data
 */
static void
info_obj_object_hdr(struct pmem_info *pip, int v, int vid,
	void *ptr, uint64_t id)
{
	struct pmemobjpool *pop = pip->obj.pop;
	struct oob_header *oob = OOB_HEADER_FROM_PTR(ptr);
	struct allocation_header *alloc = PTR_TO_ALLOC_HDR(ptr);
	void *data = ptr;

	outv_nl(vid);
	outv_field(vid, "Object", "%lu", id);
	outv_field(vid, "Offset", "0x%016lx", PTR_TO_OFF(pop, data));

	int vahdr = v && pip->args.obj.valloc;
	int voobh = v && pip->args.obj.voobhdr;

	outv_indent(vahdr || voobh, 1);

	info_obj_alloc_hdr(pip, vahdr, alloc);
	info_obj_oob_hdr(pip, voobh, oob);

	outv_hexdump(v && pip->args.vdata, data,
			alloc->size, PTR_TO_OFF(pip->obj.pop, data), 1);

	outv_indent(vahdr || voobh, -1);
}

/*
 * set_entry_cb -- callback for set objects from undo log
 */
static void
set_entry_cb(struct pmem_info *pip, int v, int vid, void *ptr,
	size_t i)
{
	struct tx_range *range = ptr;
	info_obj_object_hdr(pip, v, vid, ptr, i);

	outv_title(v, "Tx range");
	outv_indent(v, 1);
	outv_field(v, "Offset", "0x%016lx", range->offset);
	outv_field(v, "Size", "%s", out_get_size_str(range->size,
				pip->args.human));
	outv_indent(v, -1);
	outv_nl(v);
}

/*
 * set_entry_cache_cb -- callback for set cache objects from undo log
 */
static void
set_entry_cache_cb(struct pmem_info *pip, int v, int vid,
	void *ptr, size_t i)
{
	struct tx_range_cache *cache = ptr;
	info_obj_object_hdr(pip, v, vid, ptr, i);

	int title = 0;
	for (int i = 0; i < MAX_CACHED_RANGES; ++i) {
		struct tx_range *range = (struct tx_range *)&cache->range[i];
		if (range->offset == 0 || range->size == 0)
			break;

		if (!title) {
			outv_title(v, "Tx range cache");
			outv_indent(v, 1);
			title = 1;
		}
		outv(v, "%010u: Offset: 0x%016lx Size: %s\n", i, range->offset,
			out_get_size_str(range->size, pip->args.human));
	}
	if (title)
		outv_indent(v, -1);
}

/*
 * info_obj_lane_tx -- print transaction's lane section
 */
static void
info_obj_lane_tx(struct pmem_info *pip, int v,
	struct lane_section_layout *layout)
{
	struct lane_tx_layout *section = (struct lane_tx_layout *)layout;

	outv_field(v, "State", "%s", out_get_tx_state_str(section->state));

	int vobj = v && (pip->args.obj.valloc || pip->args.obj.voobhdr);
	info_obj_pvector(pip, v, vobj, &section->undo_log[UNDO_ALLOC],
			"Undo Log - alloc", info_obj_object_hdr);
	info_obj_pvector(pip, v, vobj, &section->undo_log[UNDO_FREE],
			"Undo Log - free", info_obj_object_hdr);
	info_obj_pvector(pip, v, v, &section->undo_log[UNDO_SET],
			"Undo Log - set", set_entry_cb);
	info_obj_pvector(pip, v, v, &section->undo_log[UNDO_SET_CACHE],
			"Undo Log - set cache", set_entry_cache_cb);

}

/*
 * info_obj_lane_section -- print lane's section
 */
static void
info_obj_lane_section(struct pmem_info *pip, int v, struct lane_layout *lane,
	enum lane_section_type type)
{
	if (!(pip->args.obj.lane_sections & (1U << type)))
		return;

	outv_nl(v);
	outv_field(v, "Lane section", "%s", out_get_lane_section_str(type));

	size_t lane_off = PTR_TO_OFF(pip->obj.pop, &lane->sections[type]);
	outv_hexdump(v && pip->args.vhdrdump, &lane->sections[type],
			LANE_SECTION_LEN, lane_off, 1);

	outv_indent(v, 1);
	switch (type) {
	case LANE_SECTION_ALLOCATOR:
		info_obj_lane_alloc(v, &lane->sections[type]);
		break;
	case LANE_SECTION_LIST:
		info_obj_lane_list(pip, v, &lane->sections[type]);
		break;
	case LANE_SECTION_TRANSACTION:
		info_obj_lane_tx(pip, v, &lane->sections[type]);
		break;
	default:
		break;
	}
	outv_indent(v, -1);
}

/*
 * info_obj_lanes -- print lanes structures
 */
static void
info_obj_lanes(struct pmem_info *pip)
{
	int v = pip->args.obj.vlanes;

	if (!outv_check(v))
		return;

	struct pmemobjpool *pop = pip->obj.pop;
	/*
	 * Iterate through all lanes from specified range and print
	 * specified sections.
	 */
	struct lane_layout *lanes = (void *)((char *)pip->obj.pop +
			pop->lanes_offset);
	struct range *curp = NULL;
	FOREACH_RANGE(curp, &pip->args.obj.lane_ranges) {
		for (uint64_t i = curp->first;
			i <= curp->last && i < pop->nlanes; i++) {

			/* For -R check print lane only if needs recovery */
			if (pip->args.obj.lanes_recovery &&
				!lane_need_recovery(pip, &lanes[i]))
				continue;

			outv_title(v, "Lane", "%d", i);

			outv_indent(v, 1);

			info_obj_lane_section(pip, v, &lanes[i],
					LANE_SECTION_ALLOCATOR);
			info_obj_lane_section(pip, v, &lanes[i],
					LANE_SECTION_LIST);
			info_obj_lane_section(pip, v, &lanes[i],
					LANE_SECTION_TRANSACTION);

			outv_indent(v, -1);
		}
	}
}

/*
 * info_obj_heap -- print pmemobj heap headers
 */
static void
info_obj_heap(struct pmem_info *pip)
{
	int v = pip->args.obj.vheap;
	struct pmemobjpool *pop = pip->obj.pop;
	struct heap_layout *layout = OFF_TO_PTR(pop, pop->heap_offset);
	struct heap_header *heap = &layout->header;

	outv(v, "\nPMEMOBJ Heap Header:\n");
	outv_hexdump(v && pip->args.vhdrdump, heap, sizeof(*heap),
			pop->heap_offset, 1);

	outv_field(v, "Signature", "%s", heap->signature);
	outv_field(v, "Major", "%ld", heap->major);
	outv_field(v, "Minor", "%ld", heap->minor);
	outv_field(v, "Size", "%s",
			out_get_size_str(heap->size, pip->args.human));
	outv_field(v, "Chunk size", "%s",
			out_get_size_str(heap->chunksize, pip->args.human));
	outv_field(v, "Chunks per zone", "%ld", heap->chunks_per_zone);
	outv_field(v, "Checksum", "%s", out_get_checksum(heap, sizeof(*heap),
				&heap->checksum));
}

/*
 * info_obj_zone -- print information about zone
 */
static void
info_obj_zone_hdr(struct pmem_info *pip, int v, struct zone_header *zone)
{
	outv_hexdump(v && pip->args.vhdrdump, zone, sizeof(*zone),
			PTR_TO_OFF(pip->obj.pop, zone), 1);
	outv_field(v, "Magic", "%s", out_get_zone_magic_str(zone->magic));
	outv_field(v, "Size idx", "%u", zone->size_idx);
}

/*
 * info_obj_object -- print information about object
 */
static void
info_obj_object(struct pmem_info *pip, struct obj_header *objh,
	uint64_t objid)
{
	uint64_t real_size = objh->ahdr.size - sizeof(struct obj_header);

	if (!util_ranges_contain(&pip->args.ranges, objid))
		return;

	if (!util_ranges_contain(&pip->args.obj.type_ranges,
			objh->oobh.type_num))
		return;

	if (!util_ranges_contain(&pip->args.obj.zone_ranges,
			objh->ahdr.zone_id))
		return;

	if (!util_ranges_contain(&pip->args.obj.chunk_ranges,
			objh->ahdr.chunk_id))
		return;

	pip->obj.stats.n_total_objects++;
	pip->obj.stats.n_total_bytes += real_size;

	struct pmem_obj_type_stats *type_stats =
		pmem_obj_stats_get_type(&pip->obj.stats, objh->oobh.type_num);

	type_stats->n_objects++;
	type_stats->n_bytes += real_size;


	int vid = pip->args.obj.vobjects;
	int v = pip->args.obj.vobjects;

	outv_indent(v, 1);
	info_obj_object_hdr(pip, v, vid, OBJH_TO_PTR(objh), objid);
	outv_indent(v, -1);
}

/*
 * info_obj_run_objects -- print information about objects from chunk run
 */
static void
info_obj_run_objects(struct pmem_info *pip, int v, struct chunk_run *run)
{
	uint32_t bsize = get_bitmap_size(run);
	uint32_t i = 0;
	while (i < bsize) {
		uint32_t nval = i / BITS_PER_VALUE;
		uint64_t bval = run->bitmap[nval];
		uint32_t nbit = i % BITS_PER_VALUE;

		if (!(bval & (1ULL << nbit))) {
			i++;
			continue;
		}

		struct obj_header *objh =
			(struct obj_header *)&run->data[run->block_size * i];

		/* skip root object */
		if (!objh->oobh.size) {
			info_obj_object(pip, objh, pip->obj.objid);
			pip->obj.objid++;
		}

		i += (uint32_t)(objh->ahdr.size / run->block_size);
	}
}

/*
 * info_obj_run_bitmap -- print chunk run's bitmap
 */
static void
info_obj_run_bitmap(int v, struct chunk_run *run)
{
	uint32_t bsize = get_bitmap_size(run);

	if (outv_check(v) && outv_check(VERBOSE_MAX)) {
		/* print all values from bitmap for higher verbosity */
		for (int i = 0; i < MAX_BITMAP_VALUES; i++) {
			outv(VERBOSE_MAX, "%s\n",
					get_bitmap_str(run->bitmap[i],
						BITS_PER_VALUE));
		}
	} else {
		/* print only used values for lower verbosity */
		uint32_t i;
		for (i = 0; i < bsize / BITS_PER_VALUE; i++)
			outv(v, "%s\n", get_bitmap_str(run->bitmap[i],
						BITS_PER_VALUE));

		unsigned mod = bsize % BITS_PER_VALUE;
		if (mod != 0) {
			outv(v, "%s\n", get_bitmap_str(run->bitmap[i], mod));
		}
	}
}

/*
 * info_obj_chunk -- print chunk info
 */
static void
info_obj_chunk(struct pmem_info *pip, uint64_t c,
	struct chunk_header *chunk_hdr, struct chunk *chunk,
	struct pmem_obj_zone_stats *stats)
{
	int v = pip->args.obj.vchunkhdr;
	outv(v, "\n");
	outv_field(v, "Chunk", "%lu", c);

	struct pmemobjpool *pop = pip->obj.pop;

	outv_hexdump(v && pip->args.vhdrdump, chunk_hdr, sizeof(*chunk_hdr),
			PTR_TO_OFF(pop, chunk_hdr), 1);

	outv_field(v, "Type", "%s", out_get_chunk_type_str(chunk_hdr->type));
	outv_field(v, "Flags", "0x%x %s", chunk_hdr->flags,
			out_get_chunk_flags(chunk_hdr->flags));
	outv_field(v, "Size idx", "%lu", chunk_hdr->size_idx);

	if (chunk_hdr->type == CHUNK_TYPE_USED ||
		chunk_hdr->type == CHUNK_TYPE_FREE) {
		stats->class_stats[DEFAULT_BUCKET].n_units +=
			chunk_hdr->size_idx;

		if (chunk_hdr->type == CHUNK_TYPE_USED) {
			stats->class_stats[DEFAULT_BUCKET].n_used +=
				chunk_hdr->size_idx;

			struct obj_header *objh =
				(struct obj_header *)chunk->data;

			/* skip root object */
			if (!objh->oobh.size) {
				info_obj_object(pip, objh, pip->obj.objid);
				pip->obj.objid++;
			}
		}

	} else if (chunk_hdr->type == CHUNK_TYPE_RUN) {
		struct chunk_run *run = (struct chunk_run *)chunk;

		outv_hexdump(v && pip->args.vhdrdump, run,
				sizeof(run->block_size) + sizeof(run->bitmap),
				PTR_TO_OFF(pop, run), 1);

		int class = heap_size_to_class(run->block_size);
		if (class >= 0 && class < MAX_CLASS_STATS) {
			outv_field(v, "Block size", "%s",
					out_get_size_str(run->block_size,
						pip->args.human));

			uint32_t units = get_bitmap_size(run);
			uint32_t used = 0;
			if (get_bitmap_reserved(run,  &used)) {
				outv_field(v, "Bitmap", "[error]");
			} else {
				stats->class_stats[class].n_units += units;
				stats->class_stats[class].n_used += used;

				outv_field(v, "Bitmap", "%u / %u", used, units);
			}

			info_obj_run_bitmap(v && pip->args.obj.vbitmap, run);
			info_obj_run_objects(pip, v && pip->args.obj.vobjects,
					run);
		} else {
			outv_field(v, "Block size", "%s [invalid!]",
					out_get_size_str(run->block_size,
						pip->args.human));
		}
	}
}

/*
 * info_obj_zone_chunks -- print chunk headers from specified zone
 */
static void
info_obj_zone_chunks(struct pmem_info *pip, struct zone *zone,
	struct pmem_obj_zone_stats *stats)
{
	uint64_t c = 0;
	while (c < zone->header.size_idx) {
		enum chunk_type type = zone->chunk_headers[c].type;
		uint64_t size_idx = zone->chunk_headers[c].size_idx;
		if (util_ranges_contain(&pip->args.obj.chunk_ranges, c)) {
			if (pip->args.obj.chunk_types & (1U << type)) {
				stats->n_chunks++;
				stats->n_chunks_type[type]++;

				stats->size_chunks += size_idx;
				stats->size_chunks_type[type] += size_idx;

				info_obj_chunk(pip, c, &zone->chunk_headers[c],
						&zone->chunks[c], stats);

			}

			if (size_idx > 1 && type != CHUNK_TYPE_RUN &&
				pip->args.obj.chunk_types &
				(1 << CHUNK_TYPE_FOOTER)) {
				size_t f = c + size_idx - 1;
				info_obj_chunk(pip, f, &zone->chunk_headers[f],
					&zone->chunks[f], stats);
			}
		}

		c += size_idx;
	}
}

/*
 * info_obj_root_obj -- print root object
 */
static void
info_obj_root_obj(struct pmem_info *pip)
{
	int v = pip->args.obj.vroot;

	struct pmemobjpool *pop = pip->obj.pop;
	if (!pop->root_offset) {
		outv(v, "\nNo root object...\n");
		return;
	}

	void *data = OFF_TO_PTR(pop, pop->root_offset);
	struct obj_header *objh = OBJH_FROM_PTR(data);

	outv_title(v, "Root object");
	outv_field(v, "Offset", "0x%016x", PTR_TO_OFF(pop, data));
	uint64_t root_size = objh->oobh.size & ~OBJ_INTERNAL_OBJECT_MASK;
	outv_field(v, "Size",
			out_get_size_str(root_size, pip->args.human));

	/* do not print object id and offset for root object */
	info_obj_object_hdr(pip, v, VERBOSE_SILENT, OBJH_TO_PTR(objh), 0);
}

/*
 * info_obj_zones -- print zones and chunks
 */
static void
info_obj_zones_chunks(struct pmem_info *pip)
{
	if (!outv_check(pip->args.obj.vheap) &&
		!outv_check(pip->args.vstats) &&
		!outv_check(pip->args.obj.vobjects))
		return;

	struct pmemobjpool *pop = pip->obj.pop;
	struct heap_layout *layout = OFF_TO_PTR(pop, pop->heap_offset);
	size_t maxzone = util_heap_max_zone(pop->heap_size);
	pip->obj.stats.n_zones = maxzone;
	pip->obj.stats.zone_stats = calloc(maxzone,
			sizeof(struct pmem_obj_zone_stats));
	if (!pip->obj.stats.zone_stats)
		err(1, "Cannot allocate memory for zone stats");

	for (size_t i = 0; i < maxzone; i++) {
		struct zone *zone = ZID_TO_ZONE(layout, i);

		if (util_ranges_contain(&pip->args.obj.zone_ranges, i)) {
			int vvv = pip->args.obj.vheap &&
				(pip->args.obj.vzonehdr ||
				pip->args.obj.vchunkhdr);

			outv_title(vvv, "Zone", "%lu", i);

			if (zone->header.magic == ZONE_HEADER_MAGIC)
				pip->obj.stats.n_zones_used++;

			info_obj_zone_hdr(pip, pip->args.obj.vheap &&
					pip->args.obj.vzonehdr,
					&zone->header);

			outv_indent(vvv, 1);
			info_obj_zone_chunks(pip, zone,
					&pip->obj.stats.zone_stats[i]);
			outv_indent(vvv, -1);
		}
	}
}

/*
 * info_obj_descriptor -- print pmemobj descriptor
 */
static void
info_obj_descriptor(struct pmem_info *pip)
{
	int v = VERBOSE_DEFAULT;

	if (!outv_check(v))
		return;

	outv(v, "\nPMEM OBJ Header:\n");
	struct pmemobjpool *pop = pip->obj.pop;

	uint8_t *hdrptr = (uint8_t *)pop + sizeof(pop->hdr);
	size_t hdrsize = sizeof(*pop) - sizeof(pop->hdr);
	size_t hdroff = sizeof(pop->hdr);
	outv_hexdump(pip->args.vhdrdump, hdrptr, hdrsize, hdroff, 1);

	/* check if layout is zeroed */
	char *layout = util_check_memory((uint8_t *)pop->layout,
			sizeof(pop->layout), 0) ?
			pop->layout : "(null)";

	/* address for checksum */
	void *dscp = (void *)((uintptr_t)(&pop->hdr) +
			sizeof(struct pool_hdr));

	outv_field(v, "Layout", layout);
	outv_field(v, "Lanes offset", "0x%lx", pop->lanes_offset);
	outv_field(v, "Number of lanes", "%lu", pop->nlanes);
	outv_field(v, "Heap offset", "0x%lx", pop->heap_offset);
	outv_field(v, "Heap size", "%lu", pop->heap_size);
	outv_field(v, "Checksum", "%s", out_get_checksum(dscp, OBJ_DSC_P_SIZE,
				&pop->checksum));
	outv_field(v, "Root offset", "0x%lx", pop->root_offset);

	/* run id with -v option */
	outv_field(v + 1, "Run id", "%lu", pop->run_id);
}

/*
 * info_obj_stats_objjects -- print objects' statistics
 */
static void
info_obj_stats_objects(struct pmem_info *pip, int v,
	struct pmem_obj_stats *stats)
{
	outv_field(v, "Number of objects", "%lu",
			stats->n_total_objects);
	outv_field(v, "Number of bytes", "%s", out_get_size_str(
			stats->n_total_bytes, pip->args.human));

	outv_title(v, "Objects by type");

	outv_indent(v, 1);
	struct pmem_obj_type_stats *type_stats;
	TAILQ_FOREACH(type_stats, &pip->obj.stats.type_stats, next) {
		if (!type_stats->n_objects)
			continue;

		double n_objects_perc = 100.0 *
			(double)type_stats->n_objects /
			(double)stats->n_total_objects;
		double n_bytes_perc = 100.0 *
			(double)type_stats->n_bytes /
			(double)stats->n_total_bytes;

		outv_nl(v);
		outv_field(v, "Type number", "%lu", type_stats->type_num);
		outv_field(v, "Number of objects", "%lu [%s]",
			type_stats->n_objects,
			out_get_percentage(n_objects_perc));
		outv_field(v, "Number of bytes", "%s [%s]",
			out_get_size_str(
				type_stats->n_bytes,
				pip->args.human),
			out_get_percentage(n_bytes_perc));
	}
	outv_indent(v, -1);
}

/*
 * info_boj_stats_alloc_classes -- print allocation classes' statistics
 */
static void
info_obj_stats_alloc_classes(struct pmem_info *pip, int v,
	struct pmem_obj_zone_stats *stats)
{
	uint64_t total_bytes = 0;
	uint64_t total_used = 0;

	outv_indent(v, 1);
	for (int class = 0; class < MAX_CLASS_STATS; class++) {
		uint64_t class_size = heap_class_to_size(class);
		double used_perc = 100.0 *
			(double)stats->class_stats[class].n_used /
			(double)stats->class_stats[class].n_units;

		if (!stats->class_stats[class].n_units)
			continue;
		outv_nl(v);
		outv_field(v, "Unit size", "%s", out_get_size_str(
					class_size, pip->args.human));
		outv_field(v, "Units", "%lu",
				stats->class_stats[class].n_units);
		outv_field(v, "Used units", "%lu [%s]",
				stats->class_stats[class].n_used,
				out_get_percentage(used_perc));

		uint64_t bytes = class_size *
			stats->class_stats[class].n_units;
		uint64_t used = class_size *
			stats->class_stats[class].n_used;

		total_bytes += bytes;
		total_used += used;

		double used_bytes_perc = 100.0 * (double)used / (double)bytes;

		outv_field(v, "Bytes", "%s",
				out_get_size_str(bytes, pip->args.human));
		outv_field(v, "Used bytes", "%s [%s]",
				out_get_size_str(used, pip->args.human),
				out_get_percentage(used_bytes_perc));
	}
	outv_indent(v, -1);

	double used_bytes_perc = total_bytes ? 100.0 *
		(double)total_used / (double)total_bytes : 0.0;

	outv_nl(v);
	outv_field(v, "Total bytes", "%s",
			out_get_size_str(total_bytes, pip->args.human));
	outv_field(v, "Total used bytes", "%s [%s]",
			out_get_size_str(total_used, pip->args.human),
			out_get_percentage(used_bytes_perc));
}

/*
 * info_obj_stats_chunks -- print chunks' statistics
 */
static void
info_obj_stats_chunks(struct pmem_info *pip, int v,
	struct pmem_obj_zone_stats *stats)
{
	outv_field(v, "Number of chunks", "%lu", stats->n_chunks);

	outv_indent(v, 1);
	for (unsigned type = 0; type < MAX_CHUNK_TYPE; type++) {
		double type_perc = 100.0 *
			(double)stats->n_chunks_type[type] /
			(double)stats->n_chunks;
		if (stats->n_chunks_type[type]) {
			outv_field(v, out_get_chunk_type_str(type),
				"%lu [%s]",
				stats->n_chunks_type[type],
				out_get_percentage(type_perc));
		}
	}
	outv_indent(v, -1);

	outv_nl(v);
	outv_field(v, "Total chunks size", "%s", out_get_size_str(
				stats->size_chunks, pip->args.human));

	outv_indent(v, 1);
	for (unsigned type = 0; type < MAX_CHUNK_TYPE; type++) {
		double type_perc = 100.0 *
			(double)stats->size_chunks_type[type] /
			(double)stats->size_chunks;
		if (stats->size_chunks_type[type]) {
			outv_field(v, out_get_chunk_type_str(type),
				"%lu [%s]",
				stats->size_chunks_type[type],
				out_get_percentage(type_perc));
		}

	}
	outv_indent(v, -1);
}

/*
 * info_obj_add_zone_stats -- add stats to total
 */
static void
info_obj_add_zone_stats(struct pmem_obj_zone_stats *total,
	struct pmem_obj_zone_stats *stats)
{
	total->n_chunks += stats->n_chunks;
	total->size_chunks += stats->size_chunks;

	for (int type = 0; type < MAX_CHUNK_TYPE; type++) {
		total->n_chunks_type[type] +=
			stats->n_chunks_type[type];
		total->size_chunks_type[type] +=
			stats->size_chunks_type[type];
	}

	for (int class = 0; class < MAX_BUCKETS; class++) {
		total->class_stats[class].n_units +=
			stats->class_stats[class].n_units;
		total->class_stats[class].n_used +=
			stats->class_stats[class].n_used;
	}
}

/*
 * info_obj_stats_zones -- print zones' statistics
 */
static void
info_obj_stats_zones(struct pmem_info *pip, int v, struct pmem_obj_stats *stats,
	struct pmem_obj_zone_stats *total)
{
	double used_zones_perc = 100.0 * (double)stats->n_zones_used /
		(double)stats->n_zones;

	outv_field(v, "Number of zones", "%lu", stats->n_zones);
	outv_field(v, "Number of used zones", "%lu [%s]", stats->n_zones_used,
			out_get_percentage(used_zones_perc));

	outv_indent(v, 1);
	for (uint64_t i = 0; i < stats->n_zones_used; i++) {
		outv_title(v, "Zone", "%lu", i);

		struct pmem_obj_zone_stats *zstats = &stats->zone_stats[i];

		info_obj_stats_chunks(pip, v, zstats);

		outv_title(v, "Zone's allocation classes");
		info_obj_stats_alloc_classes(pip, v, zstats);

		info_obj_add_zone_stats(total, zstats);
	}
	outv_indent(v, -1);
}

/*
 * info_obj_stats -- print statistics
 */
static void
info_obj_stats(struct pmem_info *pip)
{
	int v = pip->args.vstats;

	if (!outv_check(v))
		return;

	struct pmem_obj_stats *stats = &pip->obj.stats;
	struct pmem_obj_zone_stats total;
	memset(&total, 0, sizeof(total));

	outv_title(v, "Statistics");

	outv_title(v, "Objects");
	info_obj_stats_objects(pip, v, stats);

	outv_title(v, "Heap");
	info_obj_stats_zones(pip, v, stats, &total);

	if (stats->n_zones_used > 1) {
		outv_title(v, "Total zone's statistics");
		outv_title(v, "Chunks statistics");
		info_obj_stats_chunks(pip, v, &total);

		outv_title(v, "Allocation classes");
		info_obj_stats_alloc_classes(pip, v, &total);
	}

}

static struct pmem_info *Pip;

static void
info_obj_sa_sigaction(int signum, siginfo_t *info, void *context)
{
	uintptr_t offset = (uintptr_t)info->si_addr - (uintptr_t)Pip->obj.pop;
	outv_err("Invalid offset 0x%lx\n", offset);
	exit(EXIT_FAILURE);
}

static struct sigaction info_obj_sigaction = {
	.sa_sigaction = info_obj_sa_sigaction,
	.sa_flags = SA_SIGINFO
};

/*
 * info_obj -- print information about obj pool type
 */
int
pmempool_info_obj(struct pmem_info *pip)
{
	pip->obj.pop = pool_set_file_map(pip->pfile, 0);
	if (pip->obj.pop == NULL)
		return -1;

	pip->obj.size = pip->pfile->size;

	Pip = pip;
	if (sigaction(SIGSEGV, &info_obj_sigaction, NULL)) {
		perror("sigaction");
		return -1;
	}

	pip->obj.uuid_lo = pmemobj_get_uuid_lo(pip->obj.pop);

	info_obj_descriptor(pip);
	info_obj_lanes(pip);
	info_obj_root_obj(pip);
	info_obj_heap(pip);
	info_obj_zones_chunks(pip);
	info_obj_stats(pip);

	return 0;
}
