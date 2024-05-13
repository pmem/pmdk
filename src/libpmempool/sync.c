// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2024, Intel Corporation */

/*
 * sync.c -- a module for poolset synchronizing
 */

#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>

#include "libpmem.h"
#include "replica.h"
#include "out.h"
#include "os.h"
#include "util_pmem.h"
#include "util.h"

#define BB_DATA_STR "offset 0x%zx, length 0x%zx, nhealthy %i"

/* defines 'struct bb_vec' - the vector of the 'struct bad_block' structures */
VEC(bb_vec, struct bad_block);

/*
 * validate_args -- (internal) check whether passed arguments are valid
 */
static int
validate_args(struct pool_set *set)
{
	LOG(3, "set %p", set);
	ASSERTne(set, NULL);

	/* the checks below help detect use of incorrect poolset file */

	/*
	 * check if all parts in the poolset are large enough
	 * (now replication works only for pmemobj pools)
	 */
	if (replica_check_part_sizes(set, PMEMOBJ_MIN_POOL)) {
		CORE_LOG_ERROR("part sizes check failed");
		goto err;
	}

	/*
	 * check if all directories for part files exist
	 */
	if (replica_check_part_dirs(set)) {
		CORE_LOG_ERROR("part directories check failed");
		goto err;
	}

	return 0;

err:
	if (errno == 0)
		errno = EINVAL;
	return -1;
}

/*
 * sync_copy_data -- (internal) copy data from the healthy replica
 *                   to the broken one
 */
static int
sync_copy_data(void *src_addr, void *dst_addr, size_t off, size_t len,
		struct pool_replica *rep_h,
		struct pool_replica *rep, const struct pool_set_part *part)
{
	LOG(3, "src_addr %p dst_addr %p off %zu len %zu "
		"rep_h %p rep %p part %p",
		src_addr, dst_addr, off, len, rep_h, rep, part);

	LOG(10,
		"copying data (offset 0x%zx length 0x%zx) from local replica -- '%s'",
		off, len, rep_h->part[0].path);

	/* copy all data */
	memcpy(dst_addr, src_addr, len);
	util_persist(part->is_dev_dax, dst_addr, len);

	return 0;
}

/*
 * sync_recreate_header -- (internal) recreate the header
 */
static int
sync_recreate_header(struct pool_set *set, unsigned r, unsigned p,
			struct pool_hdr *src_hdr)
{
	LOG(3, "set %p replica %u part %u src_hdr %p", set, r, p, src_hdr);

	struct pool_attr attr;
	util_pool_hdr2attr(&attr, src_hdr);

	if (util_header_create(set, r, p, &attr, 1) != 0) {
		CORE_LOG_ERROR(
			"part headers create failed for replica %u part %u",
			r, p);
		errno = EINVAL;
		return -1;
	}

	return 0;
}

/*
 * sync_mark_replica_no_badblocks -- (internal) mark replica as not having
 *                                              bad blocks
 */
static void
sync_mark_replica_no_badblocks(unsigned repn,
				struct poolset_health_status *set_hs)
{
	LOG(3, "repn %u set_hs %p", repn, set_hs);

	struct replica_health_status *rhs = REP_HEALTH(set_hs, repn);

	if (rhs->flags & HAS_BAD_BLOCKS) {
		rhs->flags &= ~HAS_BAD_BLOCKS;
		LOG(4, "replica %u has no bad blocks now", repn);
	}
}

/*
 * sync_mark_part_no_badblocks -- (internal) mark part as not having bad blocks
 */
static void
sync_mark_part_no_badblocks(unsigned repn, unsigned partn,
				struct poolset_health_status *set_hs)
{
	LOG(3, "repn %u partn %u set_hs %p", repn, partn, set_hs);

	struct replica_health_status *rhs = REP_HEALTH(set_hs, repn);

	if (rhs->part[PART_HEALTHidx(rhs, partn)].flags & HAS_BAD_BLOCKS) {
		rhs->part[PART_HEALTHidx(rhs, partn)].flags &= ~HAS_BAD_BLOCKS;
		LOG(4, "replica %u part %u has no bad blocks now", repn, partn);
	}
}

/*
 * sync_recalc_badblocks -- (internal) recalculate offset and length
 *                          of bad blocks to absolute ones
 *                          (relative to the beginning of the pool)
 */
static int
sync_recalc_badblocks(struct pool_set *set,
			struct poolset_health_status *set_hs)
{
	LOG(3, "set %p set_hs %p", set, set_hs);

	/* header size for all headers but the first one */
	size_t hdrsize = (set->options & (OPTION_SINGLEHDR | OPTION_NOHDRS)) ?
				0 : Mmap_align;

	for (unsigned r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = REP(set, r);
		struct replica_health_status *rep_hs = set_hs->replica[r];

		for (unsigned p = 0; p < rep->nparts; ++p) {

			struct part_health_status *phs = &rep_hs->part[p];

			if (!replica_part_has_bad_blocks(phs)) {
				/* skip parts with no bad blocks */
				continue;
			}

			ASSERTne(phs->bbs.bb_cnt, 0);
			ASSERTne(phs->bbs.bbv, NULL);

			LOG(10, "Replica %u part %u HAS %u bad blocks",
				r, p, phs->bbs.bb_cnt);

			size_t part_off = replica_get_part_offset(set, r, p);

			for (unsigned i = 0; i < phs->bbs.bb_cnt; i++) {
				LOG(10,
					"relative bad block #%i: offset %zu, length %zu",
					i,
					phs->bbs.bbv[i].offset,
					phs->bbs.bbv[i].length);

				size_t off = phs->bbs.bbv[i].offset;
				size_t len = phs->bbs.bbv[i].length;

				if (len + off <= hdrsize)
					continue;

				/* parts #>0 are mapped without the header */
				if (p > 0 && hdrsize > 0) {
					if (off >= hdrsize) {
						/*
						 * Bad block does not overlap
						 * with the header, so only
						 * adjust the offset.
						 */
						off -= hdrsize;
					} else {
						/*
						 * Bad block overlaps
						 * with the header,
						 * so adjust the length
						 * and zero the offset.
						 */
						len -= hdrsize - off;
						off = 0;
					}
				}

				replica_align_badblock_offset_length(&off, &len,
								set, r, p);

				phs->bbs.bbv[i].offset = part_off + off;
				phs->bbs.bbv[i].length = (unsigned)len;

				LOG(10,
					"absolute bad block #%i: offset 0x%zx, length 0x%zx",
					i,
					phs->bbs.bbv[i].offset,
					phs->bbs.bbv[i].length);
			}
		}
	}

	return 0;
}

/*
 * sync_badblocks_find_healthy_replica -- (internal) look for a healthy replica
 *                                                   for each bad block
 *
 * This function looks for a healthy replica for each bad block. Bad blocks
 * can overlap across replicas, so each bad block may have to be divided
 * into smaller parts which can be fixed using different healthy replica.
 *
 * Key variables:
 * - bbv_all[] - array containing all (possibly divided) bad blocks
 *               from all previous replicas.
 * - bbv_aux[] - array containing all (possibly divided) bad blocks
 *               from all previous parts of the current replica merged with
 *               these bad blocks from bbv_all[] that have offsets less or equal
 *               the greatest bad block's offset in the previous part.
 *
 * This function merges bad blocks from bbv_all[] with bad blocks
 * from the current part and writes the outcome bad blocks to bbv_aux[].
 * Only bad blocks with offsets less or equal the greatest bad block's offset
 * in the current part will be moved from bbv_all[] to bbv_aux[].
 * The rest of them has to be moved at the end by sync_badblocks_move_vec().
 *
 * bbv_aux[] becomes new bbv_all[] and bbv_aux[] is zeroed
 * before checking the next replica (bbv_all = bbv_aux; bbv_aux = 0).
 *
 * For example (all replicas have only one part):
 * - bbv_all with rep#0: |__----___________----__|
 * - merged with  rep#1: |____----_______----____|
 * - gives such bbv_aux: |__11--00_______00--11__|
 * - merged with  rep#2: |__________---__________|
 * - gives such bbv_aux: |__112200__000__002211__| (all bad blocks can be fixed)
 *
 * where:
 *   '_' stands for a healthy block (no bad block)
 *   '-' stands for a bad block with nhealthy == NO_HEALTHY_REPLICA
 *   'N' stands for a bad block with nhealthy == N (can be fixed using rep#N)
 */
static int
sync_badblocks_find_healthy_replica(struct part_health_status *phs,
					int rep,
					struct bb_vec *pbbv_all,
					struct bb_vec *pbbv_aux,
					unsigned *i_all)
{
	LOG(3, "phs %p rep %i pbbv_all %p pbbv_aux %p i_all %i",
		phs, rep, pbbv_all, pbbv_aux, *i_all);

	struct bad_block bb_add;	/* the element which is being added  */
	struct bad_block bb_new;	/* a new element */
	struct bad_block *pbb_all;	/* current element of bbv_all[] */

	unsigned long long beg_prev;
	unsigned long long end_prev;
	unsigned long long beg_new;
	unsigned long long end_new;
	size_t len_prev;
	size_t len_new;

	size_t size_all = VEC_SIZE(pbbv_all);

	if (size_all == 0) {
		/* there were no bad blocks so far, so fill up bbv_aux[] */
		for (unsigned i = 0; i < phs->bbs.bb_cnt; i++) {
			bb_add = phs->bbs.bbv[i];

			if (rep > 0)
				/* bad block can be fixed with replica #0 */
				bb_add.nhealthy = 0;

			if (VEC_PUSH_BACK(pbbv_aux, bb_add))
				return -1;

			LOG(10,
				"added bad block (prev-empty): " BB_DATA_STR,
				bb_add.offset, bb_add.length, bb_add.nhealthy);
		}
	} else {
		if (*i_all < size_all) {
			pbb_all = VEC_GET(pbbv_all, (*i_all)++);
		} else {
			pbb_all = NULL;
		}

		for (unsigned i = 0; i < phs->bbs.bb_cnt; i++) {
			bb_new = phs->bbs.bbv[i];

			LOG(10,
				" * (%u) inserting new bad block: " BB_DATA_STR,
				i + 1,
				bb_new.offset, bb_new.length, bb_new.nhealthy);

			if (pbb_all == NULL || pbb_all->length == 0) {
				if (*i_all < size_all)
					pbb_all = VEC_GET(pbbv_all, (*i_all)++);
				else
					pbb_all = NULL;
			}

			/* all from bbv_all before the bb_new */
			while (pbb_all != NULL && pbb_all->offset
							+ pbb_all->length - 1
							< bb_new.offset) {
				if (pbb_all->nhealthy == NO_HEALTHY_REPLICA)
					/* can be fixed with this replica */
					pbb_all->nhealthy = rep;

				if (VEC_PUSH_BACK(pbbv_aux, *pbb_all))
					return -1;

				LOG(10,
					"added bad block (prev-before): "
					BB_DATA_STR,
					pbb_all->offset, pbb_all->length,
					pbb_all->nhealthy);

				if (*i_all < size_all) {
					pbb_all = VEC_GET(pbbv_all, (*i_all)++);
				} else {
					pbb_all = NULL;
					break;
				}
			}

			beg_new = bb_new.offset;
			len_new = bb_new.length;
			end_new = beg_new + len_new - 1;

			/* all pbb_all overlapping with the bb_new */
			while (len_new > 0 && pbb_all != NULL) {

				beg_prev = pbb_all->offset;
				len_prev = pbb_all->length;
				end_prev = beg_prev + len_prev - 1;

				/* check if new overlaps with prev */
				if (end_prev < beg_new || end_new < beg_prev)
					break;

				/*
				 * 1st part: non-overlapping part
				 * of pbb_all or bb_new
				 */
				if (beg_prev < beg_new) {
					/* non-overlapping part of pbb_all */
					bb_add.offset = beg_prev;
					bb_add.length = (unsigned)
							(beg_new - beg_prev);

					if (pbb_all->nhealthy !=
							NO_HEALTHY_REPLICA) {
						bb_add.nhealthy =
							pbb_all->nhealthy;
					} else {
						/*
						 * It can be fixed with
						 * this replica.
						 */
						bb_add.nhealthy = rep;
					}

					if (VEC_PUSH_BACK(pbbv_aux, bb_add))
						return -1;

					LOG(10,
						"added bad block (prev-only): "
						BB_DATA_STR,
						bb_add.offset, bb_add.length,
						bb_add.nhealthy);

					beg_prev += bb_add.length;
					len_prev -= bb_add.length;

				} else if (beg_new < beg_prev) {
					/* non-overlapping part of bb_new */
					bb_add.offset = beg_new;
					bb_add.length = (unsigned)
							(beg_prev - beg_new);

					if (rep == 0) {
						bb_add.nhealthy =
							NO_HEALTHY_REPLICA;
					} else {
						/*
						 * It can be fixed with any
						 * previous replica, so let's
						 * choose replia #0.
						 */
						bb_add.nhealthy = 0;
					}

					if (VEC_PUSH_BACK(pbbv_aux, bb_add))
						return -1;

					LOG(10,
						"added bad block (new-only): "
						BB_DATA_STR,
						bb_add.offset, bb_add.length,
						bb_add.nhealthy);

					beg_new += bb_add.length;
					len_new -= bb_add.length;
				}

				/*
				 * 2nd part: overlapping part
				 * of pbb_all and bb_new
				 */
				if (len_prev <= len_new) {
					bb_add.offset = beg_prev;
					bb_add.length = len_prev;

					beg_new += len_prev;
					len_new -= len_prev;

					/* whole pbb_all was added */
					len_prev = 0;
				} else {
					bb_add.offset = beg_new;
					bb_add.length = len_new;

					beg_prev += len_new;
					len_prev -= len_new;

					/* whole bb_new was added */
					len_new = 0;
				}

				bb_add.nhealthy = pbb_all->nhealthy;

				if (VEC_PUSH_BACK(pbbv_aux, bb_add))
					return -1;

				LOG(10,
					"added bad block (common): "
					BB_DATA_STR,
					bb_add.offset, bb_add.length,
					bb_add.nhealthy);

				/* update pbb_all */
				pbb_all->offset = beg_prev;
				pbb_all->length = len_prev;

				if (len_prev == 0) {
					if (*i_all < size_all)
						pbb_all = VEC_GET(pbbv_all,
								(*i_all)++);
					else
						pbb_all = NULL;
				}
			}

			/* the rest of the bb_new */
			if (len_new > 0) {
				bb_add.offset = beg_new;
				bb_add.length = len_new;

				if (rep > 0)
					/* it can be fixed with replica #0 */
					bb_add.nhealthy = 0;
				else
					bb_add.nhealthy = NO_HEALTHY_REPLICA;

				if (VEC_PUSH_BACK(pbbv_aux, bb_add))
					return -1;

				LOG(10,
					"added bad block (new-rest): "
					BB_DATA_STR,
					bb_add.offset, bb_add.length,
					bb_add.nhealthy);
			}
		}

		if (pbb_all != NULL && pbb_all->length > 0 && *i_all > 0)
			/* this pbb_all will be used again in the next part */
			(*i_all)--;
	}

	return 0;
}

/*
 * sync_badblocks_assign_healthy_replica -- (internal) assign healthy replica
 *                                                   for each bad block
 */
static int
sync_badblocks_assign_healthy_replica(struct part_health_status *phs,
					int rep,
					struct bb_vec *pbbv_all,
					unsigned *i_all)
{
	LOG(3, "phs %p rep %i pbbv_all %p i_all %i",
		phs, rep, pbbv_all, *i_all);

	struct bad_block bb_new;	/* a new element */
	struct bad_block bb_old;	/* an old element */
	struct bad_block *pbb_all;	/* current element of bbv_all[] */

	size_t length_left;

	struct bb_vec bbv_new = VEC_INITIALIZER;

#ifdef DEBUG /* variables required for ASSERTs below */
	size_t size_all = VEC_SIZE(pbbv_all);
#endif
	pbb_all = VEC_GET(pbbv_all, *i_all);

	for (unsigned i = 0; i < phs->bbs.bb_cnt; i++) {
		bb_old = phs->bbs.bbv[i];

		LOG(10,
			"assigning old bad block: " BB_DATA_STR,
			bb_old.offset, bb_old.length, bb_old.nhealthy);

		/*
		 * Skip all bad blocks from bbv_all with offsets
		 * less than the offset of the current bb_old.
		 */
		while (pbb_all->offset < bb_old.offset) {
			/* (*i_all) has to be less than (size_all - 1) */
			ASSERT(*i_all < size_all - 1);
			pbb_all = VEC_GET(pbbv_all, ++(*i_all));
		}

		bb_new.offset = bb_old.offset;
		length_left = bb_old.length;

		while (length_left > 0) {
			LOG(10,
				"checking saved bad block: " BB_DATA_STR,
				pbb_all->offset, pbb_all->length,
				pbb_all->nhealthy);

			ASSERTeq(pbb_all->offset, bb_new.offset);
			ASSERT(pbb_all->length <= length_left);

			bb_new.length = pbb_all->length;
			bb_new.nhealthy = pbb_all->nhealthy;

			if (VEC_PUSH_BACK(&bbv_new, bb_new))
				goto error_exit;

			LOG(10,
				"added new bad block: " BB_DATA_STR,
				bb_new.offset, bb_new.length, bb_new.nhealthy);

			bb_new.offset += bb_new.length;
			length_left -= bb_new.length;

			if (length_left == 0)
				continue;

			/* (*i_all) has to be less than (size_all - 1) */
			ASSERT(*i_all < size_all - 1);
			pbb_all = VEC_GET(pbbv_all, ++(*i_all));
		}
	}

	Free(phs->bbs.bbv);
	phs->bbs.bbv = VEC_ARR(&bbv_new);
	phs->bbs.bb_cnt = (unsigned)VEC_SIZE(&bbv_new);

	LOG(10, "added %u new bad blocks", phs->bbs.bb_cnt);

	return 0;

error_exit:
	VEC_DELETE(&bbv_new);
	return -1;
}

/*
 * sync_badblocks_move_vec -- (internal) move bad blocks from vector pbbv_all
 *                                       to vector pbbv_aux
 */
static int
sync_badblocks_move_vec(struct bb_vec *pbbv_all,
			struct bb_vec *pbbv_aux,
			unsigned i_all,
			unsigned rep)
{
	LOG(3, "pbbv_all %p pbbv_aux %p i_all %u rep  %u",
		pbbv_all, pbbv_aux, i_all, rep);

	size_t size_all = VEC_SIZE(pbbv_all);
	struct bad_block *pbb_all;

	while (i_all < size_all) {
		pbb_all = VEC_GET(pbbv_all, i_all++);

		if (pbb_all->length == 0)
			continue;

		if (pbb_all->nhealthy == NO_HEALTHY_REPLICA && rep > 0)
			/* it can be fixed using the last replica */
			pbb_all->nhealthy = (int)rep;

		if (VEC_PUSH_BACK(pbbv_aux, *pbb_all))
			return -1;

		LOG(10,
			"added bad block (prev-after): " BB_DATA_STR,
			pbb_all->offset, pbb_all->length,
			pbb_all->nhealthy);
	}

	return 0;
}

/*
 * sync_check_bad_blocks_overlap -- (internal) check if there are uncorrectable
 *                                  bad blocks (bad blocks overlapping
 *                                  in all replicas)
 */
static int
sync_check_bad_blocks_overlap(struct pool_set *set,
				struct poolset_health_status *set_hs)
{
	LOG(3, "set %p set_hs %p", set, set_hs);

	struct bb_vec bbv_all = VEC_INITIALIZER;
	struct bb_vec bbv_aux = VEC_INITIALIZER;

	int ret = -1;

	for (unsigned r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = REP(set, r);
		struct replica_health_status *rep_hs = set_hs->replica[r];

		unsigned i_all = 0;	/* index in bbv_all */

		for (unsigned p = 0; p < rep->nparts; ++p) {
			struct part_health_status *phs = &rep_hs->part[p];

			if (!replica_part_has_bad_blocks(phs)) {
				/* skip parts with no bad blocks */
				continue;
			}

			ASSERTne(phs->bbs.bb_cnt, 0);
			ASSERTne(phs->bbs.bbv, NULL);

			LOG(10, "Replica %u part %u HAS %u bad blocks",
				r, p, phs->bbs.bb_cnt);

			/*
			 * This function merges bad blocks from bbv_all
			 * with bad blocks from the current part
			 * and writes the outcome bad blocks to bbv_aux.
			 * Only bad blocks with offsets less or equal
			 * the greatest bad block's offset in the current part
			 * will be moved from bbv_all to bbv_aux.
			 * The rest of them has to be moved at the end
			 * by sync_badblocks_move_vec() below.
			 */
			if (sync_badblocks_find_healthy_replica(phs, (int)r,
							&bbv_all, &bbv_aux,
							&i_all))
				goto exit;
		}

		/*
		 * Move the rest of bad blocks from bbv_all to bbv_aux
		 * (for more details see the comment above).
		 * All these bad blocks can be fixed using the last replica 'r'.
		 */
		if (sync_badblocks_move_vec(&bbv_all, &bbv_aux, i_all, r))
			return -1;

		/* bbv_aux becomes a new bbv_all */
		VEC_MOVE(&bbv_all, &bbv_aux);
		i_all = 0;
	}

	ret = 0;

	/* check if there is an uncorrectable bad block */
	size_t size_all = VEC_SIZE(&bbv_all);
	for (unsigned i = 0; i < size_all; i++) {
		struct bad_block *pbb_all = VEC_GET(&bbv_all, i);
		if (pbb_all->nhealthy == NO_HEALTHY_REPLICA) {
			ret = 1; /* this bad block cannot be fixed */

			CORE_LOG_ERROR(
				"uncorrectable bad block found: offset 0x%zx, length 0x%zx",
				pbb_all->offset, pbb_all->length);

			goto exit;
		}
	}

	/*
	 * All bad blocks can be fixed,
	 * so assign healthy replica for each of them.
	 */
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = REP(set, r);
		struct replica_health_status *rep_hs = set_hs->replica[r];

		if (!replica_has_bad_blocks(r, set_hs)) {
			/* skip replicas with no bad blocks */
			continue;
		}

		unsigned i_all = 0;	/* index in bbv_all */

		for (unsigned p = 0; p < rep->nparts; ++p) {
			struct part_health_status *phs = &rep_hs->part[p];

			if (!replica_part_has_bad_blocks(phs)) {
				/* skip parts with no bad blocks */
				continue;
			}

			if (sync_badblocks_assign_healthy_replica(phs, (int)r,
								&bbv_all,
								&i_all))
				goto exit;
		}
	}

exit:
	VEC_DELETE(&bbv_aux);
	VEC_DELETE(&bbv_all);

	return ret;
}

/*
 * sync_badblocks_data -- (internal) clear bad blocks in replica
 */
static int
sync_badblocks_data(struct pool_set *set, struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, set_hs %p", set, set_hs);

	struct pool_replica *rep_h;

	for (unsigned r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = REP(set, r);
		struct replica_health_status *rep_hs = set_hs->replica[r];

		for (unsigned p = 0; p < rep->nparts; ++p) {

			struct part_health_status *phs = &rep_hs->part[p];

			if (!replica_part_has_bad_blocks(phs)) {
				/* skip parts with no bad blocks */
				continue;
			}

			ASSERTne(phs->bbs.bb_cnt, 0);
			ASSERTne(phs->bbs.bbv, NULL);

			const struct pool_set_part *part = &rep->part[p];
			size_t part_off = replica_get_part_offset(set, r, p);

			for (unsigned i = 0; i < phs->bbs.bb_cnt; i++) {
				size_t off = phs->bbs.bbv[i].offset - part_off;
				size_t len = phs->bbs.bbv[i].length;

				ASSERT(phs->bbs.bbv[i].nhealthy >= 0);

				rep_h = REP(set,
					(unsigned)phs->bbs.bbv[i].nhealthy);

				void *src_addr = ADDR_SUM(rep_h->part[0].addr,
								part_off + off);
				void *dst_addr = ADDR_SUM(part->addr, off);

				if (sync_copy_data(src_addr, dst_addr,
							part_off + off, len,
							rep_h, rep, part))
					return -1;
			}

			/* free array of bad blocks */
			Free(phs->bbs.bbv);
			phs->bbs.bbv = NULL;

			/* mark part as having no bad blocks */
			sync_mark_part_no_badblocks(r, p, set_hs);
		}

		/* mark replica as having no bad blocks */
		sync_mark_replica_no_badblocks(r, set_hs);
	}

	CORE_LOG_HARK("all bad blocks have been fixed");

	if (replica_remove_all_recovery_files(set_hs)) {
		CORE_LOG_ERROR("removing bad block recovery files failed");
		return -1;
	}

	return 0;
}

/*
 * recreate_broken_parts -- (internal) create parts in place of the broken ones
 */
static int
recreate_broken_parts(struct pool_set *set,
			struct poolset_health_status *set_hs,
			int fix_bad_blocks)
{
	LOG(3, "set %p set_hs %p fix_bad_blocks %i",
		set, set_hs, fix_bad_blocks);

	for (unsigned r = 0; r < set_hs->nreplicas; ++r) {

		struct pool_replica *broken_r = set->replica[r];

		for (unsigned p = 0; p < set_hs->replica[r]->nparts; ++p) {
			/* skip unbroken parts */
			if (!replica_is_part_broken(r, p, set_hs))
				continue;

			/* remove parts from broken replica */
			if (replica_remove_part(set, r, p, fix_bad_blocks)) {
				CORE_LOG_ERROR("cannot remove part");
				return -1;
			}

			/* create removed part and open it */
			if (util_part_open(&broken_r->part[p], 0,
						1 /* create */)) {
				CORE_LOG_ERROR("cannot open/create parts");
				return -1;
			}

			sync_mark_part_no_badblocks(r, p, set_hs);
		}
	}

	return 0;
}

/*
 * fill_struct_part_uuids -- (internal) set part uuids in pool_set structure
 */
static void
fill_struct_part_uuids(struct pool_set *set, unsigned repn,
		struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, repn %u, set_hs %p", set, repn, set_hs);
	struct pool_replica *rep = REP(set, repn);
	struct pool_hdr *hdrp;
	for (unsigned p = 0; p < rep->nhdrs; ++p) {
		/* skip broken parts */
		if (replica_is_part_broken(repn, p, set_hs))
			continue;

		hdrp = HDR(rep, p);
		memcpy(rep->part[p].uuid, hdrp->uuid, POOL_HDR_UUID_LEN);
	}
}

/*
 * is_uuid_already_used -- (internal) check if given uuid is assigned to
 *                         any of the earlier replicas
 */
static int
is_uuid_already_used(uuid_t uuid, struct pool_set *set, unsigned repn)
{
	for (unsigned r = 0; r < repn; ++r) {
		if (uuidcmp(uuid, PART(REP(set, r), 0)->uuid) == 0)
			return 1;
	}
	return 0;
}

/*
 * fill_struct_broken_part_uuids -- (internal) set part uuids in pool_set
 *                                  structure
 */
static int
fill_struct_broken_part_uuids(struct pool_set *set, unsigned repn,
		struct poolset_health_status *set_hs, unsigned flags)
{
	LOG(3, "set %p, repn %u, set_hs %p, flags %u", set, repn, set_hs,
			flags);
	struct pool_replica *rep = REP(set, repn);
	struct pool_hdr *hdrp;
	for (unsigned p = 0; p < rep->nhdrs; ++p) {
		/* skip unbroken parts */
		if (!replica_is_part_broken(repn, p, set_hs))
			continue;

		/* check if part was damaged or was added by transform */
		if (replica_is_poolset_transformed(flags)) {
			/* generate new uuid for this part */
			if (util_uuid_generate(rep->part[p].uuid) < 0) {
				ERR_WO_ERRNO(
					"cannot generate pool set part UUID");
				errno = EINVAL;
				return -1;
			}
			continue;
		}

		if (!replica_is_part_broken(repn, p - 1, set_hs) &&
				!(set->options & OPTION_SINGLEHDR)) {
			/* try to get part uuid from the previous part */
			hdrp = HDRP(rep, p);
			memcpy(rep->part[p].uuid, hdrp->next_part_uuid,
					POOL_HDR_UUID_LEN);
		} else if (!replica_is_part_broken(repn, p + 1, set_hs) &&
				!(set->options & OPTION_SINGLEHDR)) {
			/* try to get part uuid from the next part */
			hdrp = HDRN(rep, p);
			memcpy(rep->part[p].uuid, hdrp->prev_part_uuid,
					POOL_HDR_UUID_LEN);
		} else if (p == 0 &&
			!replica_is_part_broken(repn - 1, 0, set_hs)) {
			/* try to get part uuid from the previous replica */
			hdrp = HDR(REPP(set, repn), 0);
			if (is_uuid_already_used(hdrp->next_repl_uuid, set,
					repn)) {
				ERR_WO_ERRNO(
					"repeated uuid - some replicas were created with a different poolset file");
				errno = EINVAL;
				return -1;
			}
			memcpy(rep->part[p].uuid, hdrp->next_repl_uuid,
						POOL_HDR_UUID_LEN);
		} else if (p == 0 &&
			!replica_is_part_broken(repn + 1, 0, set_hs)) {
			/* try to get part uuid from the next replica */
			hdrp = HDR(REPN(set, repn), 0);
			if (is_uuid_already_used(hdrp->prev_repl_uuid, set,
					repn)) {
				ERR_WO_ERRNO(
					"repeated uuid - some replicas were created with a different poolset file");
				errno = EINVAL;
				return -1;
			}
			memcpy(rep->part[p].uuid, hdrp->prev_repl_uuid,
						POOL_HDR_UUID_LEN);
		} else {
			/* generate new uuid for this part */
			if (util_uuid_generate(rep->part[p].uuid) < 0) {
				ERR_WO_ERRNO(
					"cannot generate pool set part UUID");
				errno = EINVAL;
				return -1;
			}
		}
	}
	return 0;
}

/*
 * fill_struct_uuids -- (internal) fill fields in pool_set needed for further
 *                      altering of uuids
 */
static int
fill_struct_uuids(struct pool_set *set, unsigned src_replica,
		struct poolset_health_status *set_hs, unsigned flags)
{
	LOG(3, "set %p, src_replica %u, set_hs %p, flags %u", set, src_replica,
			set_hs, flags);

	/* set poolset uuid */
	struct pool_hdr *src_hdr0 = HDR(REP(set, src_replica), 0);
	memcpy(set->uuid, src_hdr0->poolset_uuid, POOL_HDR_UUID_LEN);

	/* set unbroken parts' uuids */
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		fill_struct_part_uuids(set, r, set_hs);
	}

	/* set broken parts' uuids */
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		if (fill_struct_broken_part_uuids(set, r, set_hs, flags))
			return -1;
	}
	return 0;
}

/*
 * create_headers_for_broken_parts -- (internal) create headers for all new
 *                                    parts created in place of the broken ones
 */
static int
create_headers_for_broken_parts(struct pool_set *set, unsigned src_replica,
		struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, src_replica %u, set_hs %p", set, src_replica, set_hs);

	struct pool_hdr *src_hdr = HDR(REP(set, src_replica), 0);

	for (unsigned r = 0; r < set_hs->nreplicas; ++r) {
		/* skip unbroken replicas */
		if (!replica_is_replica_broken(r, set_hs) &&
		    !replica_has_bad_blocks(r, set_hs))
			continue;

		for (unsigned p = 0; p < set_hs->replica[r]->nhdrs; p++) {
			/* skip unbroken parts */
			if (!replica_is_part_broken(r, p, set_hs) &&
			    !replica_part_has_corrupted_header(r, p, set_hs))
				continue;

			if (sync_recreate_header(set, r, p, src_hdr))
				return -1;
		}
	}
	return 0;
}

/*
 * copy_data_to_broken_parts -- (internal) copy data to all parts created
 *                              in place of the broken ones
 */
static int
copy_data_to_broken_parts(struct pool_set *set, unsigned healthy_replica,
		unsigned flags, struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, healthy_replica %u, flags %u, set_hs %p", set,
			healthy_replica, flags, set_hs);

	/* get pool size from healthy replica */
	size_t poolsize = set->poolsize;

	for (unsigned r = 0; r < set_hs->nreplicas; ++r) {
		/* skip unbroken and consistent replicas */
		if (replica_is_replica_healthy(r, set_hs))
			continue;

		struct pool_replica *rep = REP(set, r);
		struct pool_replica *rep_h = REP(set, healthy_replica);

		for (unsigned p = 0; p < rep->nparts; ++p) {
			/* skip unbroken parts from consistent replicas */
			if (!replica_is_part_broken(r, p, set_hs) &&
				replica_is_replica_consistent(r, set_hs))
				continue;

			const struct pool_set_part *part = &rep->part[p];

			size_t off = replica_get_part_data_offset(set, r, p);
			size_t len = replica_get_part_data_len(set, r, p);

			/* do not allow copying too much data */
			if (off >= poolsize)
				continue;

			if (off + len > poolsize)
				len = poolsize - off;

			/*
			 * First part of replica is mapped
			 * with header
			 */
			size_t fpoff = (p == 0) ? POOL_HDR_SIZE : 0;
			void *src_addr = ADDR_SUM(rep_h->part[0].addr, off);
			void *dst_addr = ADDR_SUM(part->addr, fpoff);

			if (sync_copy_data(src_addr, dst_addr, off, len,
						rep_h, rep, part))
				return -1;
		}
	}
	return 0;
}

/*
 * grant_created_parts_perm -- (internal) set RW permission rights to all
 *                            the parts created in place of the broken ones
 */
static int
grant_created_parts_perm(struct pool_set *set, unsigned src_repn,
		struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, src_repn %u, set_hs %p", set, src_repn, set_hs);

	/* choose the default permissions */
	mode_t def_mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;

	/* get permissions of the first part of the source replica */
	mode_t src_mode;
	os_stat_t sb;
	if (os_stat(PART(REP(set, src_repn), 0)->path, &sb) != 0) {
		ERR_WO_ERRNO(
			"cannot check file permissions of %s (replica %u, part %u)",
			PART(REP(set, src_repn), 0)->path, src_repn, 0);
		src_mode = def_mode;
	} else {
		src_mode = sb.st_mode;
	}

	/* set permissions to all recreated parts */
	for (unsigned r = 0; r < set_hs->nreplicas; ++r) {
		/* skip unbroken replicas */
		if (!replica_is_replica_broken(r, set_hs))
			continue;

		for (unsigned p = 0; p < set_hs->replica[r]->nparts; p++) {
			/* skip parts which were not created */
			if (!PART(REP(set, r), p)->created)
				continue;

			LOG(4, "setting permissions for part %u, replica %u",
					p, r);

			/* set rights to those of existing part files */
			if (os_chmod(PART(REP(set, r), p)->path, src_mode)) {
				ERR_WO_ERRNO(
					"cannot set permission rights for created parts: replica %u, part %u",
					r, p);
				errno = EPERM;
				return -1;
			}
		}
	}
	return 0;
}

/*
 * update_parts_linkage -- (internal) set uuids linking recreated parts within
 *                         a replica
 */
static int
update_parts_linkage(struct pool_set *set, unsigned repn,
		struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, repn %u, set_hs %p", set, repn, set_hs);
	struct pool_replica *rep = REP(set, repn);
	for (unsigned p = 0; p < rep->nhdrs; ++p) {
		struct pool_hdr *hdrp = HDR(rep, p);
		struct pool_hdr *prev_hdrp = HDRP(rep, p);
		struct pool_hdr *next_hdrp = HDRN(rep, p);

		/* set uuids in the current part */
		memcpy(hdrp->prev_part_uuid, PARTP(rep, p)->uuid,
				POOL_HDR_UUID_LEN);
		memcpy(hdrp->next_part_uuid, PARTN(rep, p)->uuid,
				POOL_HDR_UUID_LEN);
		util_checksum(hdrp, sizeof(*hdrp), &hdrp->checksum,
			1, POOL_HDR_CSUM_END_OFF(hdrp));

		/* set uuids in the previous part */
		memcpy(prev_hdrp->next_part_uuid, PART(rep, p)->uuid,
				POOL_HDR_UUID_LEN);
		util_checksum(prev_hdrp, sizeof(*prev_hdrp),
			&prev_hdrp->checksum, 1,
			POOL_HDR_CSUM_END_OFF(prev_hdrp));

		/* set uuids in the next part */
		memcpy(next_hdrp->prev_part_uuid, PART(rep, p)->uuid,
				POOL_HDR_UUID_LEN);
		util_checksum(next_hdrp, sizeof(*next_hdrp),
			&next_hdrp->checksum, 1,
			POOL_HDR_CSUM_END_OFF(next_hdrp));

		/* store pool's header */
		util_persist(PART(rep, p)->is_dev_dax, hdrp, sizeof(*hdrp));
		util_persist(PARTP(rep, p)->is_dev_dax, prev_hdrp,
				sizeof(*prev_hdrp));
		util_persist(PARTN(rep, p)->is_dev_dax, next_hdrp,
				sizeof(*next_hdrp));

	}
	return 0;
}

/*
 * update_replicas_linkage -- (internal) update uuids linking replicas
 */
static int
update_replicas_linkage(struct pool_set *set, unsigned repn)
{
	LOG(3, "set %p, repn %u", set, repn);
	struct pool_replica *rep = REP(set, repn);
	struct pool_replica *prev_r = REPP(set, repn);
	struct pool_replica *next_r = REPN(set, repn);

	ASSERT(rep->nparts > 0);
	ASSERT(prev_r->nparts > 0);
	ASSERT(next_r->nparts > 0);

	/* set uuids in the current replica */
	for (unsigned p = 0; p < rep->nhdrs; ++p) {
		struct pool_hdr *hdrp = HDR(rep, p);
		memcpy(hdrp->prev_repl_uuid, PART(prev_r, 0)->uuid,
				POOL_HDR_UUID_LEN);
		memcpy(hdrp->next_repl_uuid, PART(next_r, 0)->uuid,
				POOL_HDR_UUID_LEN);
		util_checksum(hdrp, sizeof(*hdrp), &hdrp->checksum,
			1, POOL_HDR_CSUM_END_OFF(hdrp));

		/* store pool's header */
		util_persist(PART(rep, p)->is_dev_dax, hdrp, sizeof(*hdrp));
	}

	/* set uuids in the previous replica */
	for (unsigned p = 0; p < prev_r->nhdrs; ++p) {
		struct pool_hdr *prev_hdrp = HDR(prev_r, p);
		memcpy(prev_hdrp->next_repl_uuid, PART(rep, 0)->uuid,
				POOL_HDR_UUID_LEN);
		util_checksum(prev_hdrp, sizeof(*prev_hdrp),
			&prev_hdrp->checksum, 1,
			POOL_HDR_CSUM_END_OFF(prev_hdrp));

		/* store pool's header */
		util_persist(PART(prev_r, p)->is_dev_dax, prev_hdrp,
				sizeof(*prev_hdrp));
	}

	/* set uuids in the next replica */
	for (unsigned p = 0; p < next_r->nhdrs; ++p) {
		struct pool_hdr *next_hdrp = HDR(next_r, p);

		memcpy(next_hdrp->prev_repl_uuid, PART(rep, 0)->uuid,
				POOL_HDR_UUID_LEN);
		util_checksum(next_hdrp, sizeof(*next_hdrp),
			&next_hdrp->checksum, 1,
			POOL_HDR_CSUM_END_OFF(next_hdrp));

		/* store pool's header */
		util_persist(PART(next_r, p)->is_dev_dax, next_hdrp,
				sizeof(*next_hdrp));
	}

	return 0;
}

/*
 * update_poolset_uuids -- (internal) update poolset uuid in recreated parts
 */
static int
update_poolset_uuids(struct pool_set *set, unsigned repn,
		struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, repn %u, set_hs %p", set, repn, set_hs);
	struct pool_replica *rep = REP(set, repn);
	for (unsigned p = 0; p < rep->nhdrs; ++p) {
		struct pool_hdr *hdrp = HDR(rep, p);
		memcpy(hdrp->poolset_uuid, set->uuid, POOL_HDR_UUID_LEN);
		util_checksum(hdrp, sizeof(*hdrp), &hdrp->checksum,
			1, POOL_HDR_CSUM_END_OFF(hdrp));

		/* store pool's header */
		util_persist(PART(rep, p)->is_dev_dax, hdrp, sizeof(*hdrp));
	}
	return 0;
}

/*
 * update_uuids -- (internal) set all uuids that might have changed or be unset
 *                 after recreating parts
 */
static int
update_uuids(struct pool_set *set, struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, set_hs %p", set, set_hs);
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		if (!replica_is_replica_healthy(r, set_hs))
			update_parts_linkage(set, r, set_hs);

		update_replicas_linkage(set, r);
		update_poolset_uuids(set, r, set_hs);
	}

	return 0;
}

/*
 * sync_replica -- synchronize data across replicas within a poolset
 */
int
replica_sync(struct pool_set *set, struct poolset_health_status *s_hs,
		unsigned flags)
{
	LOG(3, "set %p, flags %u", set, flags);
	int ret = 0;
	struct poolset_health_status *set_hs = NULL;

	/* check if we already know the poolset health status */
	if (s_hs == NULL) {
		/* validate poolset before checking its health */
		if (validate_args(set))
			return -1;

		/* examine poolset's health */
		if (replica_check_poolset_health(set, &set_hs,
						1 /* called from sync */,
						flags)) {
			CORE_LOG_ERROR("poolset health check failed");
			return -1;
		}

		/* check if poolset is broken; if not, nothing to do */
		if (replica_is_poolset_healthy(set_hs)) {
			CORE_LOG_HARK("poolset is healthy");
			goto out;
		}
	} else {
		set_hs = s_hs;
	}

	/* find a replica with healthy header; it will be the source of data */
	unsigned healthy_replica = replica_find_healthy_replica(set_hs);
	unsigned healthy_header = healthy_replica;
	if (healthy_header == UNDEF_REPLICA) {
		healthy_header = replica_find_replica_healthy_header(set_hs);
		if (healthy_header == UNDEF_REPLICA) {
			ERR_WO_ERRNO("no healthy replica found");
			errno = EINVAL;
			ret = -1;
			goto out;
		}
	}

	/* in dry-run mode we can stop here */
	if (is_dry_run(flags)) {
		CORE_LOG_HARK("Sync in dry-run mode finished successfully");
		goto out;
	}

	/* recreate broken parts */
	if (recreate_broken_parts(set, set_hs, fix_bad_blocks(flags))) {
		ERR_WO_ERRNO("recreating broken parts failed");
		ret = -1;
		goto out;
	}

	/* open all part files */
	if (replica_open_poolset_part_files(set)) {
		ERR_WO_ERRNO("opening poolset part files failed");
		ret = -1;
		goto out;
	}

	/* map all replicas */
	if (util_poolset_open(set)) {
		ERR_WO_ERRNO("opening poolset failed");
		ret = -1;
		goto out;
	}

	set->poolsize = set_hs->replica[healthy_header]->pool_size;
	LOG(3, "setting the pool size (%zu) from replica #%u",
		set->poolsize, healthy_header);

	/* recalculate offset and length of bad blocks */
	if (sync_recalc_badblocks(set, set_hs)) {
		CORE_LOG_ERROR("syncing bad blocks data failed");
		ret = -1;
		goto out;
	}

	/*
	 * Check if there are uncorrectable bad blocks
	 * (bad blocks overlapping in all replicas).
	 */
	int status = sync_check_bad_blocks_overlap(set, set_hs);
	if (status == -1) {
		CORE_LOG_ERROR("checking bad blocks failed");
		ret = -1;
		goto out;
	}

	if (status == 1) {
		ERR_WO_ERRNO(
			"a part of the pool has uncorrectable errors in all replicas");
		errno = EINVAL;
		ret = -1;
		goto out;
	}

	LOG(3, "bad blocks do not overlap");

	/* sync data in bad blocks */
	if (sync_badblocks_data(set, set_hs)) {
		CORE_LOG_ERROR("syncing bad blocks data failed");
		ret = -1;
		goto out;
	}

	/* find one good replica; it will be the source of data */
	healthy_replica = replica_find_healthy_replica(set_hs);
	if (healthy_replica == UNDEF_REPLICA) {
		ERR_WO_ERRNO("no healthy replica found");
		errno = EINVAL;
		ret = -1;
		goto out;
	}

	/* update uuid fields in the set structure with part headers */
	if (fill_struct_uuids(set, healthy_replica, set_hs, flags)) {
		ERR_WO_ERRNO("gathering uuids failed");
		ret = -1;
		goto out;
	}

	/* create headers for broken parts */
	if (create_headers_for_broken_parts(set, healthy_replica, set_hs)) {
		ERR_WO_ERRNO("creating headers for broken parts failed");
		ret = -1;
		goto out;
	}

	/* check and copy data if possible */
	if (copy_data_to_broken_parts(set, healthy_replica,
			flags, set_hs)) {
		ERR_WO_ERRNO("copying data to broken parts failed");
		ret = -1;
		goto out;
	}

	/* update uuids of replicas and parts */
	if (update_uuids(set, set_hs)) {
		ERR_WO_ERRNO("updating uuids failed");
		ret = -1;
		goto out;
	}

	/* grant permissions to all created parts */
	if (grant_created_parts_perm(set, healthy_replica, set_hs)) {
		ERR_WO_ERRNO("granting permissions to created parts failed");
		ret = -1;
	}

out:
	if (s_hs == NULL)
		replica_free_poolset_health_status(set_hs);
	return ret;
}
