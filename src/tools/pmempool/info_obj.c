/*
 * Copyright 2014-2018, Intel Corporation
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
#include <inttypes.h>

#include "alloc_class.h"

#include "set.h"
#include "common.h"
#include "output.h"
#include "info.h"
#include "util.h"

#define BITMAP_BUFF_SIZE 1024

#define OFF_TO_PTR(pop, off) ((void *)((uintptr_t)(pop) + (off)))

#define PTR_TO_OFF(pop, ptr) ((uintptr_t)ptr - (uintptr_t)pop)

typedef void (*pvector_callback_fn)(struct pmem_info *pip, int v, int vnum,
		const struct memory_block *m, uint64_t i);

/*
 * lane_need_recovery_redo -- return 1 if redo log needs recovery
 */
static int
lane_need_recovery_redo(struct redo_log *redo, size_t nentries)
{
	/* Needs recovery if any of redo log entries has finish flag set */
	return redo_log_nflags(redo, nentries) > 0;
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
		struct tx_range *range = (struct tx_range *)cache;

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
		ret += util_popcount64(run->bitmap[i]);
	ret += util_popcount64(run->bitmap[nvals - 1] & ~last_val);

	*reserved = ret;

	return 0;
}

#define RUN_BITMAP_SEPARATOR_DISTANCE 8

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
		if ((i + 1) % RUN_BITMAP_SEPARATOR_DISTANCE == 0)
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
		outv(v, "%010zu: "
			"Offset: 0x%016jx "
			"Value: 0x%016jx "
			"Finish flag: %d\n",
			i,
			redo_log_offset(&redo[i]),
			redo[i].value,
			redo_log_is_last(&redo[i]));
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
	struct pvector_context *ctx = pvector_new(pip->obj.pop, vec);
	if (ctx == NULL) {
		outv_err("Cannot initialize pvector context\n");
		exit(EXIT_FAILURE);
	}

	size_t nvalues = pvector_nvalues(ctx);
	outv_field(vnum, name, "%lu element%s", nvalues,
			nvalues != 1 ? "s" : "");

	outv_indent(vobj, 1);

	uint64_t i = 0;
	uint64_t off;
	struct memory_block m;
	for (off = pvector_first(ctx); off != 0; off = pvector_next(ctx)) {
		m = memblock_from_offset(pip->obj.heap, off);
		cb(pip, vobj, vobj, &m, i);
		i++;
	}

	pvector_delete(ctx);
	outv_indent(vobj, -1);
}

/*
 * info_obj_alloc_hdr -- print allocation header
 */
static void
info_obj_alloc_hdr(struct pmem_info *pip, int v,
	const struct memory_block *m)
{
	outv_title(v, "Allocation Header");

	outv_field(v, "Size", "%s", out_get_size_str(m->m_ops->get_user_size(m),
				pip->args.human));
	outv_field(v, "Extra", "%lu", m->m_ops->get_extra(m));
	outv_field(v, "Flags", "0x%x", m->m_ops->get_flags(m));
}

/*
 * info_obj_object_hdr -- print object headers and data
 */
static void
info_obj_object_hdr(struct pmem_info *pip, int v, int vid,
	const struct memory_block *m, uint64_t id)
{
	struct pmemobjpool *pop = pip->obj.pop;

	void *data = m->m_ops->get_user_data(m);

	outv_nl(vid);
	outv_field(vid, "Object", "%lu", id);
	outv_field(vid, "Offset", "0x%016lx", PTR_TO_OFF(pop, data));

	int vahdr = v && pip->args.obj.valloc;
	int voobh = v && pip->args.obj.voobhdr;

	outv_indent(vahdr || voobh, 1);

	info_obj_alloc_hdr(pip, vahdr, m);

	outv_hexdump(v && pip->args.vdata, data,
			m->m_ops->get_real_size(m),
			PTR_TO_OFF(pip->obj.pop, data), 1);

	outv_indent(vahdr || voobh, -1);

}

/*
 * set_entry_cb -- callback for set objects from undo log
 */
static void
set_entry_cb(struct pmem_info *pip, int v, int vid,
	const struct memory_block *m, uint64_t i)
{
	struct tx_range *range = m->m_ops->get_user_data(m);
	info_obj_object_hdr(pip, v, vid, m, i);

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
	const struct memory_block *m, uint64_t i)
{
	struct tx_range_cache *cache = m->m_ops->get_user_data(m);
	info_obj_object_hdr(pip, v, vid, m, i);

	int title = 0;
	uint64_t cache_size = m->m_ops->get_user_size(m);
	struct tx_range *range;

	for (uint64_t cache_offset = 0; cache_offset < cache_size; ) {
		range = (struct tx_range *)((char *)cache + cache_offset);
		if (range->offset == 0 || range->size == 0)
			break;

		if (!title) {
			outv_title(v, "Tx range cache");
			outv_indent(v, 1);
			title = 1;
		}
		outv(v, "%010" PRIu64 ": Offset: 0x%016lx Size: %s\n", i,
			range->offset, out_get_size_str(range->size,
					pip->args.human));

		cache_offset += TX_ALIGN_SIZE(range->size, TX_RANGE_MASK) +
			sizeof(struct tx_range);
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
	if (!(pip->args.obj.lane_sections & (1ULL << type)))
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

			outv_title(v, "Lane %" PRIu64, i);

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
	outv_field(v, "Chunk size", "%s",
			out_get_size_str(heap->chunksize, pip->args.human));
	outv_field(v, "Chunks per zone", "%ld", heap->chunks_per_zone);
	outv_field(v, "Checksum", "%s", out_get_checksum(heap, sizeof(*heap),
			&heap->checksum, 0));
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
info_obj_object(struct pmem_info *pip, const struct memory_block *m,
	uint64_t objid)
{
	if (!util_ranges_contain(&pip->args.ranges, objid))
		return;

	uint64_t type_num = m->m_ops->get_extra(m);

	if (!util_ranges_contain(&pip->args.obj.type_ranges, type_num))
		return;

	uint64_t real_size = m->m_ops->get_real_size(m);
	pip->obj.stats.n_total_objects++;
	pip->obj.stats.n_total_bytes += real_size;

	struct pmem_obj_type_stats *type_stats =
		pmem_obj_stats_get_type(&pip->obj.stats, type_num);

	type_stats->n_objects++;
	type_stats->n_bytes += real_size;

	int vid = pip->args.obj.vobjects;
	int v = pip->args.obj.vobjects;

	outv_indent(v, 1);
	info_obj_object_hdr(pip, v, vid, m, objid);
	outv_indent(v, -1);
}

/*
 * info_obj_run_bitmap -- print chunk run's bitmap
 */
static void
info_obj_run_bitmap(int v, struct chunk_run *run, uint32_t bsize)
{
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
 * info_obj_memblock_is_root -- (internal) checks whether the object is root
 */
static int
info_obj_memblock_is_root(struct pmem_info *pip, const struct memory_block *m)
{
	uint64_t roff = pip->obj.pop->root_offset;
	if (roff == 0)
		return 0;

	struct memory_block rm = memblock_from_offset(pip->obj.heap, roff);

	return MEMORY_BLOCK_EQUALS(*m, rm);
}

/*
 * info_obj_run_cb -- (internal) run object callback
 */
static int
info_obj_run_cb(const struct memory_block *m, void *arg)
{
	struct pmem_info *pip = arg;

	if (info_obj_memblock_is_root(pip, m))
		return 0;

	info_obj_object(pip, m, pip->obj.objid++);

	return 0;
}

/*
 * info_obj_chunk -- print chunk info
 */
static void
info_obj_chunk(struct pmem_info *pip, uint64_t c, uint64_t z,
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
	outv_field(v, "Size idx", "%u", chunk_hdr->size_idx);

	struct memory_block m = MEMORY_BLOCK_NONE;
	m.zone_id = (uint32_t)z;
	m.chunk_id = (uint32_t)c;
	m.size_idx = (uint32_t)chunk_hdr->size_idx;
	memblock_rebuild_state(pip->obj.heap, &m);

	if (chunk_hdr->type == CHUNK_TYPE_USED ||
		chunk_hdr->type == CHUNK_TYPE_FREE) {
		stats->class_stats[DEFAULT_ALLOC_CLASS_ID].n_units +=
			chunk_hdr->size_idx;

		if (chunk_hdr->type == CHUNK_TYPE_USED) {
			stats->class_stats[DEFAULT_ALLOC_CLASS_ID].n_used +=
				chunk_hdr->size_idx;

			/* skip root object */
			if (!info_obj_memblock_is_root(pip, &m)) {
				info_obj_object(pip, &m, pip->obj.objid++);
			}
		}
	} else if (chunk_hdr->type == CHUNK_TYPE_RUN) {
		struct chunk_run *run = (struct chunk_run *)chunk;

		outv_hexdump(v && pip->args.vhdrdump, run,
				sizeof(run->block_size) + sizeof(run->bitmap),
				PTR_TO_OFF(pop, run), 1);

		struct alloc_class *aclass = alloc_class_by_run(
			pip->obj.alloc_classes,
			run->block_size, m.header_type, m.size_idx);
		if (aclass) {
			outv_field(v, "Block size", "%s",
					out_get_size_str(run->block_size,
						pip->args.human));

			uint32_t units = aclass->run.bitmap_nallocs;
			uint32_t used = 0;
			if (get_bitmap_reserved(run,  &used)) {
				outv_field(v, "Bitmap", "[error]");
			} else {
				stats->class_stats[aclass->id].n_units += units;
				stats->class_stats[aclass->id].n_used += used;

				outv_field(v, "Bitmap", "%u / %u", used, units);
			}

			info_obj_run_bitmap(v && pip->args.obj.vbitmap,
				run, units);

			heap_run_foreach_object(pip->obj.heap, info_obj_run_cb,
				pip, &m);
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
info_obj_zone_chunks(struct pmem_info *pip, struct zone *zone, uint64_t z,
	struct pmem_obj_zone_stats *stats)
{
	uint64_t c = 0;
	while (c < zone->header.size_idx) {
		enum chunk_type type = zone->chunk_headers[c].type;
		uint64_t size_idx = zone->chunk_headers[c].size_idx;
		if (util_ranges_contain(&pip->args.obj.chunk_ranges, c)) {
			if (pip->args.obj.chunk_types & (1ULL << type)) {
				stats->n_chunks++;
				stats->n_chunks_type[type]++;

				stats->size_chunks += size_idx;
				stats->size_chunks_type[type] += size_idx;

				info_obj_chunk(pip, c, z,
					&zone->chunk_headers[c],
					&zone->chunks[c], stats);

			}

			if (size_idx > 1 && type != CHUNK_TYPE_RUN &&
				pip->args.obj.chunk_types &
				(1 << CHUNK_TYPE_FOOTER)) {
				size_t f = c + size_idx - 1;
				info_obj_chunk(pip, f, z,
					&zone->chunk_headers[f],
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

	outv_title(v, "Root object");
	outv_field(v, "Offset", "0x%016zx", pop->root_offset);
	uint64_t root_size = pop->root_size;
	outv_field(v, "Size", "%s",
			out_get_size_str(root_size, pip->args.human));

	struct memory_block m = memblock_from_offset(
			pip->obj.heap, pop->root_offset);

	/* do not print object id and offset for root object */
	info_obj_object_hdr(pip, v, VERBOSE_SILENT, &m, 0);
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

			outv_title(vvv, "Zone %zu", i);

			if (zone->header.magic == ZONE_HEADER_MAGIC)
				pip->obj.stats.n_zones_used++;

			info_obj_zone_hdr(pip, pip->args.obj.vheap &&
					pip->args.obj.vzonehdr,
					&zone->header);

			outv_indent(vvv, 1);
			info_obj_zone_chunks(pip, zone, i,
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
	void *dscp = (void *)((uintptr_t)(pop) + sizeof(struct pool_hdr));

	outv_field(v, "Layout", "%s", layout);
	outv_field(v, "Lanes offset", "0x%lx", pop->lanes_offset);
	outv_field(v, "Number of lanes", "%lu", pop->nlanes);
	outv_field(v, "Heap offset", "0x%lx", pop->heap_offset);
	outv_field(v, "Heap size", "%lu", pop->heap_size);
	outv_field(v, "Checksum", "%s", out_get_checksum(dscp, OBJ_DSC_P_SIZE,
			&pop->checksum, 0));
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
	for (unsigned class = 0; class < MAX_ALLOCATION_CLASSES; class++) {
		struct alloc_class *c = alloc_class_by_id(
				pip->obj.alloc_classes, (uint8_t)class);
		if (c == NULL)
			continue;
		if (!stats->class_stats[class].n_units)
			continue;

		double used_perc = 100.0 *
			(double)stats->class_stats[class].n_used /
			(double)stats->class_stats[class].n_units;

		outv_nl(v);
		outv_field(v, "Unit size", "%s", out_get_size_str(
					c->unit_size, pip->args.human));
		outv_field(v, "Units", "%lu",
				stats->class_stats[class].n_units);
		outv_field(v, "Used units", "%lu [%s]",
				stats->class_stats[class].n_used,
				out_get_percentage(used_perc));

		uint64_t bytes = c->unit_size *
			stats->class_stats[class].n_units;
		uint64_t used = c->unit_size *
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

	for (int class = 0; class < MAX_ALLOCATION_CLASSES; class++) {
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
		outv_title(v, "Zone %" PRIu64, i);

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
#ifndef _WIN32
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
#else
#define CALL_FIRST 1

static LONG CALLBACK
exception_handler(_In_ PEXCEPTION_POINTERS ExceptionInfo)
{
	PEXCEPTION_RECORD record = ExceptionInfo->ExceptionRecord;
	if (record->ExceptionCode != EXCEPTION_ACCESS_VIOLATION) {
		return EXCEPTION_CONTINUE_SEARCH;
	}
	uintptr_t offset = (uintptr_t)record->ExceptionInformation[1] -
		(uintptr_t)Pip->obj.pop;
	outv_err("Invalid offset 0x%lx\n", offset);
	exit(EXIT_FAILURE);
}
#endif

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

	struct palloc_heap *heap = calloc(1, sizeof(*heap));
	if (heap == NULL)
		err(1, "Cannot allocate memory for heap data");

	heap->layout = OFF_TO_PTR(pip->obj.pop, pip->obj.pop->heap_offset);
	heap->base = pip->obj.pop;
	pip->obj.alloc_classes = alloc_class_collection_new();
	pip->obj.heap = heap;

	Pip = pip;
#ifndef _WIN32
	if (sigaction(SIGSEGV, &info_obj_sigaction, NULL)) {
#else
	if (AddVectoredExceptionHandler(CALL_FIRST, exception_handler) ==
		NULL) {
#endif
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

	free(heap);
	alloc_class_collection_delete(pip->obj.alloc_classes);

	return 0;
}
