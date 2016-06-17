/*
 * Copyright 2016, Intel Corporation
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
 * sync.c -- module for pool set synchronizing
 */

#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

#include "sync.h"
#include "out.h"
#include "libpmemobj.h"
#include "lane.h"
#include "redo.h"
#include "memops.h"
#include "pmalloc.h"
#include "list.h"
#include "libpmempool.h"
#include "obj.h"
#include "set.h"
#include "mmap.h"

/*
 * Direction in with adjacent replica is searched
 */
enum repl_dir {
	REPLICA_DIR_PREV,	/* replica previous to current one */
	REPLICA_DIR_NEXT	/* replica next to current one */
};

/*
 * Structure keeps an information of boundaries used to copying parts
 */
struct part_modify {
	unsigned partfrom_first; /* part number we start copying from */
	unsigned partfrom_last;	/* part number we stop copying data */
	unsigned partto_first;	/* part number we start copying to */
	unsigned partto_last;	/* part number we stop copying data */

	/* offset of data in mapped parts to the beginning of root */
	uint64_t part_data_off;
	size_t part_data_len;	/* number of bytes of data to copy */

	/* flag indicating whether header was already mapped */
	bool repl_from_hdr_mapped;
};

#define ADDR_SUM(vp, lp) ((void *)((char *)(vp) + lp))
#define UNDEF_PART (-1)
#define PREV_REP_PART_NO(cpart, nparts) ((nparts + cpart - 1) % nparts)
#define NEXT_REP_PART_NO(cpart, nparts) ((nparts + cpart + 1) % nparts)
#define NEAR_REPL(r, nrepl) ((unsigned)((nrepl + (r)) % (nrepl)))

#define PAGE_ALIGNED_SIZE(size) (size & ~(Pagesize - 1))


/*
 * is_dry_run -- check whether only verification mode is enabled
 */
static inline bool
is_dry_run(struct pmempool_replica_opts *opts)
{
	return PMEMPOOL_REPLICA_VERIFY & opts->flags;
}

/*
 * is_part_removed -- check if part was removed
 */
static inline bool
is_part_removed(struct part_modify *partmodif, unsigned part)
{
	return part >= partmodif->partto_first &&
			part < partmodif->partto_last;
}

/*
 * open_replto -- open files of given replica and create lacking ones
 */
static int
open_replto(struct pool_set *set_in, struct part_modify *partmodif,
		struct pmempool_replica_opts *opts)
{
	int create = 0;
	struct pool_replica *replica = set_in->replica[opts->dst_rep];
	for (unsigned i = 0; i < replica->nparts; ++i) {
		create = is_part_removed(partmodif, i) && !is_dry_run(opts);

		if (util_poolset_file(&replica->part[i], 0, create))
			return -1;
	}
	return 0;
}

/*
 * remove_parts -- unlink parts from damaged replica
 */
static int
remove_parts(struct pool_set *set_in, unsigned repl, unsigned pstart,
		unsigned pend)
{
	struct pool_replica *replica = set_in->replica[repl];

	for (unsigned i = pstart; i < pend; ++i) {
		if (unlink(replica->part[i].path)) {
			if (errno != ENOENT)
				return -1;
		}
	}
	return 0;
}

/*
 * open_files -- open files of parts
 */
static int
open_files(struct pool_replica *repl, unsigned pstart,
		unsigned pend)
{
	/* open all parts */
	for (unsigned p = pstart; p < pend; ++p) {
		if (util_poolset_file(&repl->part[p], 0, 0)) {
			ERR("Cannot open file - part #%d", p);
			return -1;
		}
	}
	return 0;
}

/*
 * mmap_headers -- map headers
 */
static int
mmap_headers(struct pool_replica *repl, unsigned pstart,
		unsigned pend)
{
	/* map all headers - don't care about the address */
	for (unsigned p = pstart; p < pend; ++p) {
		if (util_map_hdr(&repl->part[p], MAP_SHARED) != 0) {
			ERR("Header mapping failed - part #%d", p);
			return -1;
		}
	}
	return 0;
}

/*
 * open_mmap_headers -- open parts and map headers
 */
static int
open_mmap_headers(struct pool_replica *repl, unsigned pstart,
		unsigned pend)
{
	if (open_files(repl, pstart, pend))
		return -1;
	if (mmap_headers(repl, pstart, pend))
		return -1;

	return 0;
}

/*
 * is_replica_alloc -- check if replica is on list of allocated replicas
 */
static bool
is_replica_alloc(struct replica_alloc *alocrep, unsigned replno)
{
	for (unsigned i = 0; i < alocrep->count; ++i) {
		if (alocrep->repltab[i] == replno)
			return true;
	}
	return false;
}

/*
 * add_alloc_replica -- add to list of allocated replicas
 */
static void
add_alloc_replica(struct replica_alloc *alocrep, unsigned replno)
{
	if (is_replica_alloc(alocrep, replno))
		return;

	alocrep->repltab[alocrep->count++] = replno;
}

/*
 * close_replicas -- close all opened replicas
 */
static void
close_replicas(struct replica_alloc *alocrep, struct pool_set *setin)
{
	for (unsigned i = 0; i < alocrep->count; ++i) {
		unsigned replno = alocrep->repltab[i];
		util_replica_close(setin, replno);
		struct pool_replica *repl = setin->replica[replno];
		util_part_fdclose(repl);
	}
}

/*
 * update_adjacent_repl_uuid -- set next or previous uuid in header
 */
static int
update_adjacent_repl_uuid(struct pool_replica *rep, enum repl_dir uid_dir,
		uuid_t *uuid)
{
	for (unsigned p = 0; p < rep->nparts; p++) {
		struct pool_hdr *hdrp = rep->part[p].hdr;
		if (uid_dir == REPLICA_DIR_PREV) {
			memcpy(hdrp->prev_repl_uuid, uuid, POOL_HDR_UUID_LEN);
		} else {
			memcpy(hdrp->next_repl_uuid, uuid, POOL_HDR_UUID_LEN);
		}
		util_checksum(hdrp, sizeof(*hdrp), &hdrp->checksum, 1);
	}

	return 0;
}

/*
 * update_uuids_replicas -- set next and previous uuids off all parts
 * of the adjacent replicas
 */
static int
update_uuids_replicas(struct pool_set *set_in, unsigned replto,
		struct part_modify *partmodif)
{
	struct pool_replica *replica = set_in->replica[replto];
	struct pool_replica *rprev = REP(set_in, replto - 1);
	struct pool_replica *rnext = REP(set_in, replto + 1);
	uuid_t *curr_uuid = &replica->part[0].uuid;

	update_adjacent_repl_uuid(rprev, REPLICA_DIR_NEXT, curr_uuid);
	update_adjacent_repl_uuid(rnext, REPLICA_DIR_PREV, curr_uuid);

	return 0;
}

/*
 * update_uuids -- set next and previous uuids in pool_set structure
 */
static void
update_uuids(struct pool_set *set_in, struct pmempool_replica_opts *opts,
		struct part_modify *partmodif)
{
	if (is_dry_run(opts))
		return;

	struct pool_replica *rto = set_in->replica[opts->dst_rep];
	if (partmodif->partto_first == 0)
		update_uuids_replicas(set_in, opts->dst_rep, partmodif);

	/* unless all parts were removed */
	if (partmodif->partto_last - partmodif->partto_first < rto->nparts) {

		struct pool_set_part *currpart =
				&rto->part[partmodif->partto_first];
		struct pool_set_part *part =
				&PART(rto, partmodif->partto_first - 1);
		struct pool_hdr *hdrp = part->hdr;
		memcpy(hdrp->next_part_uuid, currpart->uuid, POOL_HDR_UUID_LEN);
		util_checksum(hdrp, sizeof(*hdrp), &hdrp->checksum, 1);

		currpart = &rto->part[partmodif->partto_last - 1];
		part = &PART(rto, partmodif->partto_last);
		hdrp = part->hdr;
		memcpy(hdrp->prev_part_uuid, currpart->uuid, POOL_HDR_UUID_LEN);
		util_checksum(hdrp, sizeof(*hdrp), &hdrp->checksum, 1);
	}
}

/*
 * grant_part_perm -- set RWX permission rights to the part from range
 */
static void
grant_part_perm(struct pool_replica *repl, unsigned pstart, unsigned pend)
{
	for (unsigned p = pstart; p < pend; p++) {
		chmod(repl->part[p].path, S_IRWXU | S_IRWXG | S_IRWXO);
	}
}

/*
 * get_part_data_len -- get data length for given part
 */
static size_t
get_part_data_len(struct pool_set *set_in, unsigned repl, unsigned part)
{
	size_t len = 0;
	len = PAGE_ALIGNED_SIZE(set_in->replica[repl]->part[part].filesize) -
			POOL_HDR_SIZE;
	return len;
}

/*
 * get_part_range_data_len -- get data length in given range
 */
static size_t
get_part_range_data_len(struct pool_set *set_in, unsigned repl,
		unsigned pstart, unsigned pend)
{
	size_t len = 0;
	for (unsigned i = pstart; i < pend; ++i) {
		len += get_part_data_len(set_in, repl, i);
	}

	return len;
}

/*
 * get_part_data_offset -- get data length before given part
 */
static uint64_t
get_part_data_offset(struct pool_set *set_in, unsigned repl, unsigned part)
{
	uint64_t pdoff = 0;
	for (unsigned i = 0; i < part; ++i)
		pdoff += get_part_data_len(set_in, repl, i);

	return pdoff;
}

/*
 * fill_modif_part_list -- find parts to process (stored in part_modify
 * structure)
 */
static int
fill_modif_part_list(struct pool_set *set_in,
		struct part_modify *pmodif, unsigned repl)
{
	uint64_t start = pmodif->part_data_off;
	uint64_t end = start + pmodif->part_data_len;
	struct pool_replica *rep = set_in->replica[repl];
	uint64_t prevsize = 0;
	uint64_t currsize = 0;
	unsigned first_occu = 0;
	unsigned last_occu = 0;

	for (unsigned i = 0; i < rep->nparts; ++i) {
		currsize += get_part_data_len(set_in, repl, i);
		if (start < currsize && start >= prevsize)
			first_occu = i;
		if (end <= currsize && end >= prevsize) {
			last_occu = i;
			break;
		}
		prevsize = currsize;
	}

	if (repl == pmodif->partfrom_last) {
		pmodif->partto_first = first_occu;
		pmodif->partto_last = last_occu + 1;
	} else {
		pmodif->partfrom_first = first_occu;
		pmodif->partfrom_last = last_occu + 1;
	}
	return 0;
}

/*
 * find_parts -- fill data needed to process part copying
 */
static void
find_parts(struct pool_set *set_in, struct pmempool_replica_opts *opts,
		struct part_modify *partmodif)
{
	if (opts->src_part != UNDEF_PART) {
		partmodif->part_data_off =
				get_part_data_offset(set_in, opts->src_rep,
				(unsigned)opts->src_part);
		partmodif->part_data_len =
				get_part_data_len(set_in, opts->src_rep,
				(unsigned)opts->src_part);
		/* find parts to modify in replto */
		fill_modif_part_list(set_in, partmodif, opts->dst_rep);

		/* find new data length and offset to cover all removed parts */
		partmodif->part_data_len = get_part_range_data_len(set_in,
				opts->dst_rep, partmodif->partto_first,
				partmodif->partto_last);
		partmodif->part_data_off = get_part_data_offset(set_in,
				opts->dst_rep, partmodif->partto_first);
		fill_modif_part_list(set_in, partmodif, opts->src_rep);
	} else if (opts->dst_part != UNDEF_PART) {
		partmodif->partto_first = (unsigned)opts->dst_part;
		partmodif->partto_last = partmodif->partto_first + 1;
		partmodif->part_data_off = get_part_data_offset(set_in,
				opts->dst_rep, (unsigned)opts->dst_part);
		partmodif->part_data_len = get_part_data_len(set_in,
				opts->dst_rep, (unsigned)opts->dst_part);
		/* find parts to modify in replto */
		fill_modif_part_list(set_in, partmodif, opts->src_rep);
	} else {
		/* copy all parts */
		partmodif->partfrom_first = 0;
		partmodif->partfrom_last =
				set_in->replica[opts->src_rep]->nparts;
		partmodif->partto_first = 0;
		partmodif->partto_last =
				set_in->replica[opts->dst_rep]->nparts;
		partmodif->part_data_len  =
				set_in->replica[opts->src_rep]->repsize -
				POOL_HDR_SIZE;
	}
}

/*
 * validate_args -- check whether passed arguments are valid
 */
static int
validate_args(struct pool_set *set_in, struct pmempool_replica_opts *opts)
{
	/* check replica numbers */
	if (opts->dst_rep >= set_in->nreplicas ||
			opts->src_rep >= set_in->nreplicas ||
			opts->src_rep == opts->dst_rep) {
		ERR("No such replica number in poolset");
		return -1;
	}

	/* check part numbers */
	if (opts->dst_part != UNDEF_PART &&
		opts->src_part != UNDEF_PART) {
		ERR("partto and partfrom cannot be used in the same time");
		return -1;
	}
	return 0;
}

/*
 * map_replfrom -- map parts we copy from
 */
static int
map_replfrom(struct pool_set *set, struct part_modify *partmodif,
		unsigned repl)
{
	struct pool_replica *rep = set->replica[repl];

	/* determine a hint address for mmap() */
	void *addr = util_map_hint(partmodif->part_data_len, 0);
	if (addr == MAP_FAILED) {
		ERR("cannot find a contiguous region of given size");
		return -1;
	}

	/*
	 * map the first part we copy from and reserve space for
	 * remaining parts
	 */
	size_t mapfrom_size = get_part_range_data_len(set, repl,
			partmodif->partfrom_first, partmodif->partfrom_last);
	if (util_map_part(&rep->part[partmodif->partfrom_first],
			addr, mapfrom_size, POOL_HDR_SIZE,
			MAP_SHARED) != 0) {
		ERR("pool mapping failed");
		return -1;
	}

	size_t mapsize = PAGE_ALIGNED_SIZE(
		rep->part[partmodif->partfrom_first].filesize) - POOL_HDR_SIZE;
	addr = (char *)rep->part[partmodif->partfrom_first].addr + mapsize;

	/* map the remaining parts of the usable pool space */
	for (unsigned i = partmodif->partfrom_first + 1;
			i < partmodif->partfrom_last; ++i) {
		if (util_map_part(&rep->part[i], addr, 0, POOL_HDR_SIZE,
				MAP_SHARED | MAP_FIXED) != 0) {
			ERR("usable space mapping failed - part #%d", i);
			goto err;
		}

		mapsize += rep->part[i].size;
		addr = (char *)addr + rep->part[i].size;
	}

	ASSERTeq(mapsize, mapfrom_size);
	return 0;
err:
	util_unmap_part(&rep->part[partmodif->partfrom_first]);
	return -1;
}

/*
 * fill_struct_adjacent_uuids -- set replica uuids in pool_set structure
 */
static void
fill_struct_replica_uuids(struct pool_set *set_in, unsigned repl,
	struct part_modify *partmodif, struct pmempool_replica_opts *opts)
{
	unsigned npartno = 0;
	unsigned ppartno = 0;
	unsigned rnext = NEAR_REPL(repl + 1, (set_in)->nreplicas);
	unsigned rprev = NEAR_REPL(repl - 1, (set_in)->nreplicas);

	if (rnext == opts->src_rep)
		npartno = partmodif->partfrom_first;
	if (rprev == opts->src_rep)
		ppartno = partmodif->partfrom_first;

	struct pool_set_part *rnext_part =
			&PART(REP(set_in, repl + 1), npartno);
	struct pool_set_part *rprev_part =
			&PART(REP(set_in, repl - 1), ppartno);

	struct pool_hdr *hdrp = rnext_part->hdr;
	memcpy(rnext_part->uuid, hdrp->uuid, POOL_HDR_UUID_LEN);

	hdrp = rprev_part->hdr;
	memcpy(rprev_part->uuid, hdrp->uuid, POOL_HDR_UUID_LEN);

	/* fill up poolset uuid also */
	memcpy(set_in->uuid, hdrp->poolset_uuid, POOL_HDR_UUID_LEN);
}

/*
 * fill_struct_part_uuids -- set part uuids in pool_set structure
 */
static void
fill_struct_part_uuids(struct pool_set *set_in, struct part_modify *partmodif,
		unsigned repl)
{
	struct pool_replica *repto = set_in->replica[repl];
	if ((partmodif->partto_last - partmodif->partto_first) == repto->nparts)
		return;

	unsigned nextpart = NEXT_REP_PART_NO(partmodif->partto_last - 1,
			repto->nparts);
	unsigned prevpart = PREV_REP_PART_NO(partmodif->partto_first,
			repto->nparts);

	struct pool_hdr *hdrp;
	if (!is_part_removed(partmodif, nextpart)) {
		hdrp = repto->part[nextpart].hdr;
		memcpy(repto->part[nextpart].uuid,
				hdrp->uuid, POOL_HDR_UUID_LEN);
	}

	if (nextpart == prevpart)
		return;

	if (!is_part_removed(partmodif, prevpart)) {
		hdrp = repto->part[prevpart].hdr;
		memcpy(repto->part[prevpart].uuid,
				hdrp->uuid, POOL_HDR_UUID_LEN);
	}
}

/*
 * fill_struct_uuids -- fill fields in pool_set needed for further altering
 * of uuids
 */
static void
fill_struct_uuids(struct pool_set *set_in, struct part_modify *partmodif,
		struct pmempool_replica_opts *opts)
{
	/* fill up uuids of next and prev replica referenced to newly created */
	fill_struct_replica_uuids(set_in, opts->dst_rep, partmodif, opts);

	fill_struct_part_uuids(set_in, partmodif, opts->dst_rep);
}

/*
 * open_all_remaining_parts -- open parts that were not opened during mapping
 * data parts we copy from
 */
static int
open_all_remaining_parts(struct pool_replica *repl,
		struct part_modify *partmodif)
{
	int result = open_files(repl, 0, partmodif->partfrom_first);
	if (!result)
		result = open_files(repl, partmodif->partfrom_last,
			repl->nparts);
	return result;
}

/*
 * map_adjacent_repl_headers -- map headers of next and previous replicas
 * (needed for uuid's update)
 */
static int
map_adjacent_repl_headers(struct pool_set *set_in,
		struct pmempool_replica_opts *opts,
		struct part_modify *partmodif,
		struct replica_alloc *alocrep,
		enum repl_dir dir)
{
	unsigned last_part_rfrom;
	unsigned adjrepl_no;
	unsigned repl_no = (set_in)->nreplicas;

	int repl_calc = (int)opts->dst_rep + (dir == REPLICA_DIR_PREV ? -1 : 1);
	adjrepl_no = NEAR_REPL(repl_calc, (int)repl_no);

	struct pool_replica *adjrepl = set_in->replica[adjrepl_no];

	if (adjrepl_no == opts->src_rep) {
		/*
		 * Prev replica is that we copy from.
		 * in case where there are only two replicas next and
		 * prev are the same so map headers only once
		 */
		if (partmodif->repl_from_hdr_mapped)
			return 0;

		if (partmodif->partto_first == 0) {
			if (open_all_remaining_parts(adjrepl, partmodif))
				return -1;
			last_part_rfrom = adjrepl->nparts;
		} else {
			last_part_rfrom = partmodif->partfrom_first + 1;
		}

		if (mmap_headers(adjrepl, partmodif->partfrom_first,
				last_part_rfrom))
			return -1;

		partmodif->repl_from_hdr_mapped = true;
	} else {
		add_alloc_replica(alocrep, adjrepl_no);
		if (open_mmap_headers(adjrepl, 0,
			(partmodif->partto_first == 0 ? adjrepl->nparts : 1)))
			return -1;
	}

	return 0;
}

/*
 * map_needed_headers -- map all headers needed for future uuid's update
 */
static int
map_needed_headers(struct pool_set *set_in,
		struct pmempool_replica_opts *opts,
		struct part_modify *partmodif,
		struct replica_alloc *alocrep,
		enum repl_dir dir)
{
	/* map all needed parts from adjacent replicas */
	if (map_adjacent_repl_headers(set_in, opts, partmodif,
			alocrep, REPLICA_DIR_PREV))
		return -1;
	if (map_adjacent_repl_headers(set_in, opts, partmodif,
			alocrep, REPLICA_DIR_NEXT))
		return -1;

	return 0;
}

/*
 * calc_mapped_partfrom_off -- calculate starting address (in mapping)
 * of data we copy from
 */
static size_t
calc_mapped_partfrom_off(struct pool_set *set_in, unsigned rfrom,
		struct part_modify *partmodif)
{
	size_t fpfrom_off = get_part_range_data_len(set_in, rfrom, 0,
			partmodif->partfrom_first);

	return partmodif->part_data_off - fpfrom_off;
}

/*
 * copy_data -- check sizes and copy data into parts
 */
static int
copy_data(struct pool_set *set_in, struct pmempool_replica_opts *opts,
		struct part_modify *partmodif)
{
	size_t fpoff = 0;
	struct pool_replica *replto = set_in->replica[opts->dst_rep];
	struct pool_set_part first_partto = replto->
			part[partmodif->partto_first];
	struct pool_set_part first_partfrom = set_in->replica[opts->src_rep]->
				part[partmodif->partfrom_first];

	/* check sizes */
	if (ADDR_SUM(first_partto.addr, partmodif->part_data_len) >
		ADDR_SUM(replto->part[0].addr, replto->repsize))
		return -1;

	void *mapped_from_addr = ADDR_SUM(first_partfrom.addr,
		calc_mapped_partfrom_off(set_in, opts->src_rep, partmodif));

	/* First part of replica is mapped with header */
	if (partmodif->partto_first == 0)
		fpoff = POOL_HDR_SIZE;

	/* copy all data */
	if (!is_dry_run(opts)) {
		memcpy(ADDR_SUM(first_partto.addr, fpoff), mapped_from_addr,
			partmodif->part_data_len);
	}
	return 0;
}

/*
 * map_replto_create_hdr -- map whole replica and create headers
 */
static int
map_replto_create_hdr(struct pool_set *set_in,
		struct pmempool_replica_opts *opts,
		struct part_modify *partmodif)
{
	if (util_replica_open(set_in, opts->dst_rep, MAP_SHARED))
		return -1;

	/* update needed fields in set_in structure from part headers */
	fill_struct_uuids(set_in, partmodif, opts);

	if (!is_dry_run(opts)) {
		for (unsigned p = partmodif->partto_first;
				p < partmodif->partto_last; p++) {
			if (util_header_create(set_in, opts->dst_rep, p,
					OBJ_HDR_SIG, OBJ_FORMAT_MAJOR,
					OBJ_FORMAT_COMPAT, OBJ_FORMAT_INCOMPAT,
					OBJ_FORMAT_RO_COMPAT, NULL, NULL,
					NULL) != 0) {
				return -1;
			}
		}
	}
	return 0;
}

/*
 * sync_replica -- synchronize separate parts or whole replicas
 */
int
sync_replica(struct pool_set *set_in, struct pmempool_replica_opts *opts)
{
	int result = 0;
	struct replica_alloc alloc_rep = {.count = 0};

	struct pool_replica *rto = set_in->replica[opts->dst_rep];
	struct pool_replica *rfrom = set_in->replica[opts->src_rep];

	/* validate user arguments */
	if (validate_args(set_in, opts)) {
		errno = EINVAL;
		result = -1;
		goto error;
	}

	/* fill up structure containing info for part-copy handling */
	struct part_modify pmodif = {
		.repl_from_hdr_mapped = false
	};
	find_parts(set_in, opts, &pmodif);

	/* remove parts from damaged replica */
	if (!is_dry_run(opts)) {
		if (remove_parts(set_in, opts->dst_rep, pmodif.partto_first,
				pmodif.partto_last)) {
			ERR("Cannot remove part");
			result = -1;
			goto error;
		}
	}

	/* open all parts and create removed ones */
	if (open_replto(set_in, &pmodif, opts)) {
		ERR("Cannot open/create parts");
		result = -1;
		goto error;
	} else {
		add_alloc_replica(&alloc_rep, opts->dst_rep);
	}

	/* generate new uuids for removed parts */
	for (unsigned i = pmodif.partto_first; i < pmodif.partto_last; ++i) {
		if (util_uuid_generate(rto->part[i].uuid) < 0) {
			ERR("Cannot generate pool set part UUID");
			result = -1;
			goto error;
		}
	}

	/* open parts that we copy from */
	add_alloc_replica(&alloc_rep, opts->src_rep);
	for (unsigned i = pmodif.partfrom_first;
			i < pmodif.partfrom_last; ++i) {
		if (util_poolset_file(&rfrom->part[i], 0, 0)) {
			ERR("Cannot open file - part #%d", i);
			result = -1;
			goto error;
		}
	}

	/* map parts we copy from */
	if (map_replfrom(set_in, &pmodif, opts->src_rep) == 0) {
		add_alloc_replica(&alloc_rep, opts->src_rep);
	} else {
		ERR("Replica open failed");
		result = -1;
		goto error;
	}

	/* map needed headers for uuid altering */
	if (map_needed_headers(set_in, opts, &pmodif,
			&alloc_rep, REPLICA_DIR_PREV)) {
		ERR("Replica mmap failed");
		result = -1;
		goto error;
	}

	/* map all damaged parts and create headers */
	if (map_replto_create_hdr(set_in, opts, &pmodif)) {
		ERR("Replica creation failed");
		result = -1;
		goto error;
	}

	/* check and copy data if possible */
	if (copy_data(set_in, opts, &pmodif)) {
		ERR("Not enough memory to copy to target replica");
		result = -1;
		errno = ENOMEM;
		goto error;
	}

	/* grand permission for all created parts */
	grant_part_perm(rto, pmodif.partto_first, pmodif.partto_last);

	/* update prev and next uuids of replicas and parts */
	update_uuids(set_in, opts, &pmodif);


error:
	close_replicas(&alloc_rep, set_in);
	return result;
}
