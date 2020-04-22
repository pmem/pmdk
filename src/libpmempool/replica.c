// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2020, Intel Corporation */

/*
 * replica.c -- groups all commands for replica manipulation
 */

#include "replica.h"

#include <errno.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>

#include "obj.h"
#include "palloc.h"
#include "file.h"
#include "os.h"
#include "out.h"
#include "pool_hdr.h"
#include "set.h"
#include "util.h"
#include "uuid.h"
#include "shutdown_state.h"
#include "badblocks.h"
#include "set_badblocks.h"

/*
 * check_flags_sync -- (internal) check if flags are supported for sync
 */
static int
check_flags_sync(unsigned flags)
{
	flags &= ~(PMEMPOOL_SYNC_DRY_RUN | PMEMPOOL_SYNC_FIX_BAD_BLOCKS);
	return flags > 0;
}

/*
 * check_flags_transform -- (internal) check if flags are supported for
 *                          transform
 */
static int
check_flags_transform(unsigned flags)
{
	flags &= ~PMEMPOOL_TRANSFORM_DRY_RUN;
	return flags > 0;
}

/*
 * replica_align_badblock_offset_length -- align offset and length
 *                                          of the bad block for the given part
 */
void
replica_align_badblock_offset_length(size_t *offset, size_t *length,
			struct pool_set *set_in, unsigned repn, unsigned partn)
{
	LOG(3, "offset %zu, length %zu, pool_set %p, replica %u, part %u",
		*offset, *length, set_in, repn, partn);

	size_t alignment = set_in->replica[repn]->part[partn].alignment;

	size_t off = ALIGN_DOWN(*offset, alignment);
	size_t len = ALIGN_UP(*length + (*offset - off), alignment);

	*offset = off;
	*length = len;
}

/*
 * replica_get_part_data_len -- get data length for given part
 */
size_t
replica_get_part_data_len(struct pool_set *set_in, unsigned repn,
		unsigned partn)
{
	size_t alignment = set_in->replica[repn]->part[partn].alignment;
	size_t hdrsize = (set_in->options & OPTION_SINGLEHDR) ? 0 : alignment;
	return ALIGN_DOWN(set_in->replica[repn]->part[partn].filesize,
		alignment) - ((partn == 0) ? POOL_HDR_SIZE : hdrsize);
}

/*
 * replica_get_part_offset -- get part's offset from the beginning of replica
 */
uint64_t
replica_get_part_offset(struct pool_set *set, unsigned repn, unsigned partn)
{
	return (uint64_t)set->replica[repn]->part[partn].addr -
		(uint64_t)set->replica[repn]->part[0].addr;
}

/*
 * replica_get_part_data_offset -- get data length before given part
 */
uint64_t
replica_get_part_data_offset(struct pool_set *set, unsigned repn,
		unsigned partn)
{
	if (partn == 0)
		return POOL_HDR_SIZE;

	return (uint64_t)set->replica[repn]->part[partn].addr -
		(uint64_t)set->replica[repn]->part[0].addr;
}

/*
 * replica_remove_part -- unlink part from replica
 */
int
replica_remove_part(struct pool_set *set, unsigned repn, unsigned partn,
			int fix_bad_blocks)
{
	LOG(3, "set %p repn %u partn %u fix_bad_blocks %i",
		set, repn, partn, fix_bad_blocks);

	struct pool_set_part *part = PART(REP(set, repn), partn);
	if (part->fd != -1) {
		os_close(part->fd);
		part->fd = -1;
	}

	int olderrno = errno;
	enum file_type type = util_file_get_type(part->path);
	if (type == OTHER_ERROR)
		return -1;

	/* if the part is a device dax, clear its bad blocks */
	if (type == TYPE_DEVDAX && fix_bad_blocks &&
	    badblocks_clear_all(part->path)) {
		ERR("clearing bad blocks in device dax failed -- '%s'",
			part->path);
		errno = EIO;
		return -1;
	}

	if (type == TYPE_NORMAL && util_unlink(part->path)) {
		ERR("!removing part %u from replica %u failed",
				partn, repn);
		return -1;
	}

	errno = olderrno;
	LOG(4, "Removed part %s number %u from replica %u", part->path, partn,
			repn);
	return 0;
}

/*
 * create_replica_health_status -- (internal) create helping structure for
 *                                 storing replica's health status
 */
static struct replica_health_status *
create_replica_health_status(struct pool_set *set, unsigned repn)
{
	LOG(3, "set %p, repn %u", set, repn);

	unsigned nparts = set->replica[repn]->nparts;
	struct replica_health_status *replica_hs;

	replica_hs = Zalloc(sizeof(struct replica_health_status)
				+ nparts * sizeof(struct part_health_status));
	if (replica_hs == NULL) {
		ERR("!Zalloc for replica health status");
		return NULL;
	}

	replica_hs->nparts = nparts;
	replica_hs->nhdrs = set->replica[repn]->nhdrs;

	return replica_hs;
}

/*
 * replica_part_remove_recovery_file -- remove bad blocks' recovery file
 */
static int
replica_part_remove_recovery_file(struct part_health_status *phs)
{
	LOG(3, "phs %p", phs);

	if (phs->recovery_file_name == NULL || phs->recovery_file_exists == 0)
		return 0;

	if (os_unlink(phs->recovery_file_name) < 0) {
		ERR("!removing the bad block recovery file failed -- '%s'",
			phs->recovery_file_name);
		return -1;
	}

	LOG(3, "bad block recovery file removed -- '%s'",
		phs->recovery_file_name);

	phs->recovery_file_exists = 0;

	return 0;
}

/*
 * replica_remove_all_recovery_files -- remove all recovery files
 */
int
replica_remove_all_recovery_files(struct poolset_health_status *set_hs)
{
	LOG(3, "set_hs %p", set_hs);

	int ret = 0;

	for (unsigned r = 0; r < set_hs->nreplicas; ++r) {
		struct replica_health_status *rhs = set_hs->replica[r];
		for (unsigned p = 0; p < rhs->nparts; ++p)
			ret |= replica_part_remove_recovery_file(&rhs->part[p]);
	}

	return ret;
}

/*
 * replica_free_poolset_health_status -- free memory allocated for helping
 *                                       structure
 */
void
replica_free_poolset_health_status(struct poolset_health_status *set_hs)
{
	LOG(3, "set_hs %p", set_hs);

	for (unsigned r = 0; r < set_hs->nreplicas; ++r) {
		struct replica_health_status *rep_hs = set_hs->replica[r];

		for (unsigned p = 0; p < rep_hs->nparts; ++p) {
			Free(rep_hs->part[p].recovery_file_name);
			Free(rep_hs->part[p].bbs.bbv);
		}

		Free(set_hs->replica[r]);
	}

	Free(set_hs);
}

/*
 * replica_create_poolset_health_status -- create helping structure for storing
 *                                 poolset's health status
 */
int
replica_create_poolset_health_status(struct pool_set *set,
		struct poolset_health_status **set_hsp)
{
	LOG(3, "set %p, set_hsp %p", set, set_hsp);
	unsigned nreplicas = set->nreplicas;
	struct poolset_health_status *set_hs;
	set_hs = Zalloc(sizeof(struct poolset_health_status) +
			nreplicas * sizeof(struct replica_health_status *));
	if (set_hs == NULL) {
		ERR("!Zalloc for poolset health state");
		return -1;
	}
	set_hs->nreplicas = nreplicas;
	for (unsigned i = 0; i < nreplicas; ++i) {
		struct replica_health_status *replica_hs =
				create_replica_health_status(set, i);
		if (replica_hs == NULL) {
			replica_free_poolset_health_status(set_hs);
			return -1;
		}
		set_hs->replica[i] = replica_hs;
	}
	*set_hsp = set_hs;
	return 0;
}

/*
 * replica_is_part_broken -- check if part is marked as broken in the helping
 *                           structure
 */
int
replica_is_part_broken(unsigned repn, unsigned partn,
		struct poolset_health_status *set_hs)
{
	struct replica_health_status *rhs = REP_HEALTH(set_hs, repn);
	return (rhs->flags & IS_BROKEN) ||
		(PART_HEALTH(rhs, partn) & IS_BROKEN);
}

/*
 * is_replica_broken -- check if any part in the replica is marked as broken
 */
int
replica_is_replica_broken(unsigned repn, struct poolset_health_status *set_hs)
{
	LOG(3, "repn %u, set_hs %p", repn, set_hs);
	struct replica_health_status *r_hs = REP_HEALTH(set_hs, repn);
	if (r_hs->flags & IS_BROKEN)
		return 1;

	for (unsigned p = 0; p < r_hs->nparts; ++p) {
		if (replica_is_part_broken(repn, p, set_hs))
			return 1;
	}
	return 0;
}

/*
 * replica_is_replica_consistent -- check if replica is not marked as
 *                                  inconsistent
 */
int
replica_is_replica_consistent(unsigned repn,
		struct poolset_health_status *set_hs)
{
	return !(REP_HEALTH(set_hs, repn)->flags & IS_INCONSISTENT);
}

/*
 * replica_has_bad_blocks -- check if replica has bad blocks
 */
int
replica_has_bad_blocks(unsigned repn, struct poolset_health_status *set_hs)
{
	return REP_HEALTH(set_hs, repn)->flags & HAS_BAD_BLOCKS;
}

/*
 * replica_part_has_bad_blocks -- check if replica's part has bad blocks
 */
int
replica_part_has_bad_blocks(struct part_health_status *phs)
{
	return phs->flags & HAS_BAD_BLOCKS;
}

/*
 * replica_part_has_corrupted_header -- (internal) check if replica's part
 *                              has bad blocks in the header (corrupted header)
 */
int
replica_part_has_corrupted_header(unsigned repn, unsigned partn,
					struct poolset_health_status *set_hs)
{
	struct replica_health_status *rhs = REP_HEALTH(set_hs, repn);
	return PART_HEALTH(rhs, partn) & HAS_CORRUPTED_HEADER;
}

/*
 * replica_has_corrupted_header -- (internal) check if replica has bad blocks
 *                                 in the header (corrupted header)
 */
static int
replica_has_corrupted_header(unsigned repn,
				struct poolset_health_status *set_hs)
{
	return REP_HEALTH(set_hs, repn)->flags & HAS_CORRUPTED_HEADER;
}

/*
 * replica_is_replica_healthy -- check if replica is unbroken and consistent
 */
int
replica_is_replica_healthy(unsigned repn, struct poolset_health_status *set_hs)
{
	LOG(3, "repn %u, set_hs %p", repn, set_hs);

	int ret = !replica_is_replica_broken(repn, set_hs) &&
			replica_is_replica_consistent(repn, set_hs) &&
			!replica_has_bad_blocks(repn, set_hs);

	LOG(4, "return %i", ret);

	return ret;
}

/*
 * replica_has_healthy_header -- (internal) check if replica has healthy headers
 */
static int
replica_has_healthy_header(unsigned repn, struct poolset_health_status *set_hs)
{
	LOG(3, "repn %u, set_hs %p", repn, set_hs);

	int ret = !replica_is_replica_broken(repn, set_hs) &&
			replica_is_replica_consistent(repn, set_hs) &&
			!replica_has_corrupted_header(repn, set_hs);

	LOG(4, "return %i", ret);

	return ret;
}

/*
 * replica_is_poolset_healthy -- check if all replicas in a poolset are not
 *                               marked as broken nor inconsistent in the
 *                               helping structure
 */
int
replica_is_poolset_healthy(struct poolset_health_status *set_hs)
{
	LOG(3, "set_hs %p", set_hs);
	for (unsigned r = 0; r < set_hs->nreplicas; ++r) {
		if (!replica_is_replica_healthy(r, set_hs))
			return 0;
	}
	return 1;
}

/*
 * replica_is_poolset_transformed -- check if the flag indicating a call from
 *                                   pmempool_transform is on
 */
int
replica_is_poolset_transformed(unsigned flags)
{
	return flags & IS_TRANSFORMED;
}

/*
 * replica_find_unbroken_part_with_header -- find a part number in a given
 * replica, which is not marked as broken in the helping structure and contains
 * a pool header
 */
unsigned
replica_find_unbroken_part(unsigned repn, struct poolset_health_status *set_hs)
{
	LOG(3, "repn %u, set_hs %p", repn, set_hs);
	for (unsigned p = 0; p < REP_HEALTH(set_hs, repn)->nhdrs; ++p) {
		if (!replica_is_part_broken(repn, p, set_hs))
			return p;
	}
	return UNDEF_PART;
}

/*
 * replica_find_healthy_replica -- find a replica which is a good source of data
 */
unsigned
replica_find_healthy_replica(struct poolset_health_status *set_hs)
{
	LOG(3, "set_hs %p", set_hs);

	for (unsigned r = 0; r < set_hs->nreplicas; ++r) {
		if (replica_is_replica_healthy(r, set_hs)) {
			LOG(4, "return %i", r);
			return r;
		}
	}

	LOG(4, "return %i", UNDEF_REPLICA);
	return UNDEF_REPLICA;
}

/*
 * replica_find_replica_healthy_header -- find a replica with a healthy header
 */
unsigned
replica_find_replica_healthy_header(struct poolset_health_status *set_hs)
{
	LOG(3, "set_hs %p", set_hs);

	for (unsigned r = 0; r < set_hs->nreplicas; ++r) {
		if (replica_has_healthy_header(r, set_hs)) {
			LOG(4, "return %i", r);
			return r;
		}
	}

	LOG(4, "return %i", UNDEF_REPLICA);
	return UNDEF_REPLICA;
}

/*
 * replica_check_store_size -- (internal) store size from pool descriptor for
 * replica
 */
static int
replica_check_store_size(struct pool_set *set,
	struct poolset_health_status *set_hs, unsigned repn)
{
	LOG(3, "set %p, set_hs %p, repn %u", set, set_hs, repn);
	struct pool_replica *rep = set->replica[repn];
	struct pmemobjpool pop;

	if (rep->remote) {
		memcpy(&pop.hdr, rep->part[0].hdr, sizeof(pop.hdr));
		void *descr = (void *)((uintptr_t)&pop + POOL_HDR_SIZE);
		if (Rpmem_read(rep->remote->rpp, descr, POOL_HDR_SIZE,
			sizeof(pop) - POOL_HDR_SIZE, 0)) {
			return -1;
		}
	} else {
		/* round up map size to Mmap align size */
		if (util_map_part(&rep->part[0], NULL,
		    ALIGN_UP(sizeof(pop), rep->part[0].alignment),
		    0, MAP_SHARED, 1)) {
			return -1;
		}

		memcpy(&pop, rep->part[0].addr, sizeof(pop));

		util_unmap_part(&rep->part[0]);
	}

	void *dscp = (void *)((uintptr_t)&pop + sizeof(pop.hdr));

	if (!util_checksum(dscp, OBJ_DSC_P_SIZE, &pop.checksum, 0,
			0)) {
		set_hs->replica[repn]->flags |= IS_BROKEN;
		return 0;
	}

	set_hs->replica[repn]->pool_size = pop.heap_offset + pop.heap_size;

	return 0;
}

/*
 * check_store_all_sizes -- (internal) store sizes from pool descriptor for all
 * healthy replicas
 */
static int
check_store_all_sizes(struct pool_set *set,
	struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, set_hs %p", set, set_hs);
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		if (!replica_has_healthy_header(r, set_hs))
			continue;

		if (replica_check_store_size(set, set_hs, r))
			return -1;
	}

	return 0;
}

/*
 * check_and_open_poolset_part_files -- (internal) for each part in a poolset
 * check if the part files are accessible, and if not, mark it as broken
 * in a helping structure; then open the part file
 */
static int
check_and_open_poolset_part_files(struct pool_set *set,
		struct poolset_health_status *set_hs, unsigned flags)
{
	LOG(3, "set %p, set_hs %p, flags %u", set, set_hs, flags);
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = set->replica[r];
		struct replica_health_status *rep_hs = set_hs->replica[r];
		if (rep->remote) {
			if (util_replica_open_remote(set, r, 0)) {
				LOG(1, "cannot open remote replica no %u", r);
				return -1;
			}

			unsigned nlanes = REMOTE_NLANES;
			int ret = util_poolset_remote_open(rep, r,
					rep->repsize, 0,
					rep->part[0].addr,
					rep->resvsize, &nlanes);
			if (ret) {
				rep_hs->flags |= IS_BROKEN;
				LOG(1, "remote replica #%u marked as BROKEN",
					r);
			}

			continue;
		}

		for (unsigned p = 0; p < rep->nparts; ++p) {
			const char *path = rep->part[p].path;
			enum file_type type = util_file_get_type(path);

			if (type < 0 || os_access(path, R_OK|W_OK) != 0) {
				LOG(1, "part file %s is not accessible", path);
				errno = 0;
				rep_hs->part[p].flags |= IS_BROKEN;
				if (is_dry_run(flags))
					continue;
			}

			if (util_part_open(&rep->part[p], 0, 0)) {
				if (type == TYPE_DEVDAX) {
					LOG(1,
						"opening part on Device DAX %s failed",
						path);
					return -1;
				}
				LOG(1, "opening part %s failed", path);
				errno = 0;
				rep_hs->part[p].flags |= IS_BROKEN;
			}
		}
	}
	return 0;
}

/*
 * map_all_unbroken_headers -- (internal) map all headers in a poolset,
 *                             skipping those marked as broken in a helping
 *                             structure
 */
static int
map_all_unbroken_headers(struct pool_set *set,
		struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, set_hs %p", set, set_hs);
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = set->replica[r];
		struct replica_health_status *rep_hs = set_hs->replica[r];
		if (rep->remote)
			continue;

		for (unsigned p = 0; p < rep->nhdrs; ++p) {
			/* skip broken parts */
			if (replica_is_part_broken(r, p, set_hs))
				continue;

			LOG(4, "mapping header for part %u, replica %u", p, r);
			if (util_map_hdr(&rep->part[p], MAP_SHARED, 0) != 0) {
				LOG(1, "header mapping failed - part #%d", p);
				rep_hs->part[p].flags |= IS_BROKEN;
			}
		}
	}
	return 0;
}

/*
 * unmap_all_headers -- (internal) unmap all headers in a poolset
 */
static int
unmap_all_headers(struct pool_set *set)
{
	LOG(3, "set %p", set);
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = set->replica[r];
		util_replica_close(set, r);

		if (rep->remote && rep->remote->rpp) {
			Rpmem_close(rep->remote->rpp);
			rep->remote->rpp = NULL;
		}
	}

	return 0;
}

/*
 * check_checksums_and_signatures -- (internal) check if checksums
 *                                   and signatures are correct for parts
 *                                   in a given replica
 */
static int
check_checksums_and_signatures(struct pool_set *set,
				struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, set_hs %p", set, set_hs);
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = REP(set, r);
		struct replica_health_status *rep_hs = REP_HEALTH(set_hs, r);

		/*
		 * Checksums and signatures of remote replicas are checked
		 * during opening them on the remote side by the rpmem daemon.
		 * The local version of remote headers does not contain
		 * such data.
		 */
		if (rep->remote)
			continue;

		for (unsigned p = 0; p < rep->nhdrs; ++p) {

			/* skip broken parts */
			if (replica_is_part_broken(r, p, set_hs))
				continue;

			/* check part's checksum */
			LOG(4, "checking checksum for part %u, replica %u",
					p, r);

			struct pool_hdr *hdr = HDR(rep, p);

			if (!util_checksum(hdr, sizeof(*hdr), &hdr->checksum, 0,
					POOL_HDR_CSUM_END_OFF(hdr))) {
				ERR("invalid checksum of pool header");
				rep_hs->part[p].flags |= IS_BROKEN;
			} else if (util_is_zeroed(hdr, sizeof(*hdr))) {
					rep_hs->part[p].flags |= IS_BROKEN;
			}

			enum pool_type type = pool_hdr_get_type(hdr);
			if (type == POOL_TYPE_UNKNOWN) {
				ERR("invalid signature");
				rep_hs->part[p].flags |= IS_BROKEN;
			}
		}
	}
	return 0;
}

/*
 * replica_badblocks_recovery_file_save -- save bad blocks in the bad blocks
 *                                         recovery file before clearing them
 */
static int
replica_badblocks_recovery_file_save(struct part_health_status *part_hs)
{
	LOG(3, "part_health_status %p", part_hs);

	ASSERTeq(part_hs->recovery_file_exists, 1);
	ASSERTne(part_hs->recovery_file_name, NULL);

	struct badblocks *bbs = &part_hs->bbs;
	char *path = part_hs->recovery_file_name;
	int ret = -1;

	int fd = os_open(path, O_WRONLY | O_TRUNC);
	if (fd < 0) {
		ERR("!opening bad block recovery file failed -- '%s'", path);
		return -1;
	}

	FILE *recovery_file_name = os_fdopen(fd, "w");
	if (recovery_file_name == NULL) {
		ERR(
			"!opening a file stream for bad block recovery file failed -- '%s'",
			path);
		os_close(fd);
		return -1;
	}

	/* save bad blocks */
	for (unsigned i = 0; i < bbs->bb_cnt; i++) {
		ASSERT(bbs->bbv[i].length != 0);
		fprintf(recovery_file_name, "%zu %zu\n",
			bbs->bbv[i].offset, bbs->bbv[i].length);
	}

	if (fflush(recovery_file_name) == EOF) {
		ERR("!flushing bad block recovery file failed -- '%s'", path);
		goto exit_error;
	}

	if (os_fsync(fd) < 0) {
		ERR("!syncing bad block recovery file failed -- '%s'", path);
		goto exit_error;
	}

	/* save the finish flag */
	fprintf(recovery_file_name, "0 0\n");

	if (fflush(recovery_file_name) == EOF) {
		ERR("!flushing bad block recovery file failed -- '%s'", path);
		goto exit_error;
	}

	if (os_fsync(fd) < 0) {
		ERR("!syncing bad block recovery file failed -- '%s'", path);
		goto exit_error;
	}

	LOG(3, "bad blocks saved in the recovery file -- '%s'", path);
	ret = 0;

exit_error:
	os_fclose(recovery_file_name);

	return ret;
}

/*
 * replica_part_badblocks_recovery_file_read -- read bad blocks
 *                                             from the bad block recovery file
 *                                             for the current part
 */
static int
replica_part_badblocks_recovery_file_read(struct part_health_status *part_hs)
{
	LOG(3, "part_health_status %p", part_hs);

	ASSERT(part_hs->recovery_file_exists);
	ASSERTne(part_hs->recovery_file_name, NULL);

	VEC(bbsvec, struct bad_block) bbv = VEC_INITIALIZER;
	char *path = part_hs->recovery_file_name;
	struct bad_block bb;
	int ret = -1;

	FILE *recovery_file = os_fopen(path, "r");
	if (!recovery_file) {
		ERR("!opening the recovery file for reading failed -- '%s'",
			path);
		return -1;
	}

	unsigned long long min_offset = 0; /* minimum possible offset */

	do {
		if (fscanf(recovery_file, "%zu %zu\n",
				&bb.offset, &bb.length) < 2) {
			LOG(1, "incomplete bad block recovery file -- '%s'",
				path);
			ret = 1;
			goto error_exit;
		}

		if (bb.offset == 0 && bb.length == 0) {
			/* finish_flag */
			break;
		}

		/* check if bad blocks build an increasing sequence */
		if (bb.offset < min_offset) {
			ERR(
				"wrong format of bad block recovery file (bad blocks are not sorted by the offset in ascending order) -- '%s'",
				path);
			errno = EINVAL;
			ret = -1;
			goto error_exit;
		}

		/* update the minimum possible offset */
		min_offset = bb.offset + bb.length;

		bb.nhealthy = NO_HEALTHY_REPLICA; /* unknown healthy replica */

		/* add the new bad block to the vector */
		if (VEC_PUSH_BACK(&bbv, bb))
			goto error_exit;
	} while (1);

	part_hs->bbs.bbv = VEC_ARR(&bbv);
	part_hs->bbs.bb_cnt = (unsigned)VEC_SIZE(&bbv);

	os_fclose(recovery_file);

	LOG(1, "bad blocks read from the recovery file -- '%s'", path);

	return 0;

error_exit:
	VEC_DELETE(&bbv);
	os_fclose(recovery_file);
	return ret;
}

/* status returned by the replica_badblocks_recovery_files_check() function */
enum badblocks_recovery_files_status {
	RECOVERY_FILES_ERROR = -1,
	RECOVERY_FILES_DO_NOT_EXIST = 0,
	RECOVERY_FILES_EXIST_ALL = 1,
	RECOVERY_FILES_NOT_ALL_EXIST = 2
};

/*
 * replica_badblocks_recovery_files_check -- (internal) check if bad blocks
 *                                           recovery files exist
 */
static enum badblocks_recovery_files_status
replica_badblocks_recovery_files_check(struct pool_set *set,
					struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, set_hs %p", set, set_hs);

	int recovery_file_exists = 0;
	int recovery_file_does_not_exist = 0;

	for (unsigned r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = set->replica[r];
		struct replica_health_status *rep_hs = set_hs->replica[r];

		if (rep->remote) {
			/*
			 * Bad blocks in remote replicas currently are fixed
			 * during opening by removing and recreating
			 * the whole remote replica.
			 */
			continue;
		}

		for (unsigned p = 0; p < rep->nparts; ++p) {
			const char *path = PART(rep, p)->path;
			struct part_health_status *part_hs = &rep_hs->part[p];

			int exists = util_file_exists(path);
			if (exists < 0)
				return -1;

			if (!exists) {
				/* part file does not exist - skip it */
				continue;
			}

			part_hs->recovery_file_name =
					badblocks_recovery_file_alloc(set->path,
									r, p);
			if (part_hs->recovery_file_name == NULL) {
				LOG(1,
					"allocating name of bad block recovery file failed");
				return RECOVERY_FILES_ERROR;
			}

			exists = util_file_exists(part_hs->recovery_file_name);
			if (exists < 0)
				return -1;

			part_hs->recovery_file_exists = exists;

			if (part_hs->recovery_file_exists) {
				LOG(3, "bad block recovery file exists: %s",
					part_hs->recovery_file_name);

				recovery_file_exists = 1;

			} else {
				LOG(3,
					"bad block recovery file does not exist: %s",
					part_hs->recovery_file_name);

				recovery_file_does_not_exist = 1;
			}
		}
	}

	if (recovery_file_exists) {
		if (recovery_file_does_not_exist) {
			LOG(4, "return RECOVERY_FILES_NOT_ALL_EXIST");
			return RECOVERY_FILES_NOT_ALL_EXIST;
		} else {
			LOG(4, "return RECOVERY_FILES_EXIST_ALL");
			return RECOVERY_FILES_EXIST_ALL;
		}
	}

	LOG(4, "return RECOVERY_FILES_DO_NOT_EXIST");
	return RECOVERY_FILES_DO_NOT_EXIST;
}

/*
 * replica_badblocks_recovery_files_read -- (internal) read bad blocks from all
 *                                     bad block recovery files for all parts
 */
static int
replica_badblocks_recovery_files_read(struct pool_set *set,
					struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, set_hs %p", set, set_hs);

	int ret;

	for (unsigned r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = set->replica[r];
		struct replica_health_status *rep_hs = set_hs->replica[r];

		/* XXX: not supported yet */
		if (rep->remote)
			continue;

		for (unsigned p = 0; p < rep->nparts; ++p) {
			const char *path = PART(rep, p)->path;
			struct part_health_status *part_hs = &rep_hs->part[p];

			int exists = util_file_exists(path);
			if (exists < 0)
				return -1;

			if (!exists) {
				/* the part does not exist */
				continue;
			}

			LOG(1,
				"reading bad blocks from the recovery file -- '%s'",
				part_hs->recovery_file_name);

			ret = replica_part_badblocks_recovery_file_read(
								part_hs);
			if (ret < 0) {
				LOG(1,
					"reading bad blocks from the recovery file failed -- '%s'",
					part_hs->recovery_file_name);
				return -1;
			}

			if (ret > 0) {
				LOG(1,
					"incomplete bad block recovery file detected -- '%s'",
					part_hs->recovery_file_name);
				return 1;
			}

			if (part_hs->bbs.bb_cnt) {
				LOG(3, "part %u contains %u bad blocks -- '%s'",
					p, part_hs->bbs.bb_cnt, path);
			}
		}
	}

	return 0;
}

/*
 * replica_badblocks_recovery_files_create_empty -- (internal) create one empty
 *                                                  bad block recovery file
 *                                                  for each part file
 */
static int
replica_badblocks_recovery_files_create_empty(struct pool_set *set,
					struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, set_hs %p", set, set_hs);

	struct part_health_status *part_hs;
	const char *path;
	int fd;

	for (unsigned r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = set->replica[r];
		struct replica_health_status *rep_hs = set_hs->replica[r];

		/* XXX: not supported yet */
		if (rep->remote)
			continue;

		for (unsigned p = 0; p < rep->nparts; ++p) {
			part_hs = &rep_hs->part[p];
			path = PART(rep, p)->path;

			if (!part_hs->recovery_file_name)
				continue;

			fd = os_open(part_hs->recovery_file_name,
					O_RDWR | O_CREAT | O_EXCL,
					0600);
			if (fd < 0) {
				ERR(
					"!creating an empty bad block recovery file failed -- '%s' (part file '%s')",
					part_hs->recovery_file_name, path);
				return -1;
			}

			os_close(fd);

			char *file_name = Strdup(part_hs->recovery_file_name);
			if (file_name == NULL) {
				ERR("!Strdup");
				return -1;
			}

			char *dir_name = dirname(file_name);

			/* fsync the file's directory */
			if (os_fsync_dir(dir_name) < 0) {
				ERR(
					"!syncing the directory of the bad block recovery file failed -- '%s' (part file '%s')",
					dir_name, path);
				Free(file_name);
				return -1;
			}

			Free(file_name);

			part_hs->recovery_file_exists = 1;
		}
	}

	return 0;
}

/*
 * replica_badblocks_recovery_files_save -- (internal) save bad blocks
 *                                     in the bad block recovery files
 */
static int
replica_badblocks_recovery_files_save(struct pool_set *set,
					struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, set_hs %p", set, set_hs);

	for (unsigned r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = set->replica[r];
		struct replica_health_status *rep_hs = set_hs->replica[r];

		/* XXX: not supported yet */
		if (rep->remote)
			continue;

		for (unsigned p = 0; p < rep->nparts; ++p) {
			struct part_health_status *part_hs = &rep_hs->part[p];

			if (!part_hs->recovery_file_name)
				continue;

			int ret = replica_badblocks_recovery_file_save(part_hs);
			if (ret < 0) {
				LOG(1,
					"opening bad block recovery file failed -- '%s'",
					part_hs->recovery_file_name);
				return -1;
			}
		}
	}

	return 0;
}

/*
 * replica_badblocks_get -- (internal) get all bad blocks and save them
 *                          in part_hs->bbs structures.
 *                          Returns 1 if any bad block was found, 0 otherwise.
 */
static int
replica_badblocks_get(struct pool_set *set,
			struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, set_hs %p", set, set_hs);

	int bad_blocks_found = 0;

	for (unsigned r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = set->replica[r];
		struct replica_health_status *rep_hs = set_hs->replica[r];

		/* XXX: not supported yet */
		if (rep->remote)
			continue;

		for (unsigned p = 0; p < rep->nparts; ++p) {
			const char *path = PART(rep, p)->path;
			struct part_health_status *part_hs = &rep_hs->part[p];

			int exists = util_file_exists(path);
			if (exists < 0)
				return -1;

			if (!exists)
				continue;

			int ret = badblocks_get(path, &part_hs->bbs);
			if (ret < 0) {
				ERR(
					"!checking the pool part for bad blocks failed -- '%s'",
					path);
				return -1;
			}

			if (part_hs->bbs.bb_cnt) {
				LOG(3, "part %u contains %u bad blocks -- '%s'",
					p, part_hs->bbs.bb_cnt, path);

				bad_blocks_found = 1;
			}
		}
	}

	return bad_blocks_found;
}

/*
 * check_badblocks_in_header -- (internal) check if bad blocks corrupted
 *                              the header
 */
static int
check_badblocks_in_header(struct badblocks *bbs)
{
	for (unsigned b = 0; b < bbs->bb_cnt; b++)
		if (bbs->bbv[b].offset < POOL_HDR_SIZE)
			return 1;

	return 0;
}

/*
 * replica_badblocks_clear -- (internal) clear all bad blocks
 */
static int
replica_badblocks_clear(struct pool_set *set,
			struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, set_hs %p", set, set_hs);

	int ret;

	for (unsigned r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = set->replica[r];
		struct replica_health_status *rep_hs = set_hs->replica[r];

		/* XXX: not supported yet */
		if (rep->remote)
			continue;

		for (unsigned p = 0; p < rep->nparts; ++p) {
			const char *path = PART(rep, p)->path;
			struct part_health_status *part_hs = &rep_hs->part[p];

			int exists = util_file_exists(path);
			if (exists < 0)
				return -1;

			if (!exists) {
				/* the part does not exist */
				continue;
			}

			if (part_hs->bbs.bb_cnt == 0) {
				/* no bad blocks found */
				continue;
			}

			/* bad blocks were found */
			part_hs->flags |= HAS_BAD_BLOCKS;
			rep_hs->flags |= HAS_BAD_BLOCKS;

			if (check_badblocks_in_header(&part_hs->bbs)) {
				part_hs->flags |= HAS_CORRUPTED_HEADER;
				if (p == 0)
					rep_hs->flags |= HAS_CORRUPTED_HEADER;
			}

			ret = badblocks_clear(path, &part_hs->bbs);
			if (ret < 0) {
				LOG(1,
					"clearing bad blocks in replica failed -- '%s'",
					path);
				return -1;
			}
		}
	}

	return 0;
}

/*
 * replica_badblocks_check_or_clear -- (internal) check if replica contains
 *                                     bad blocks when in dry run
 *                                     or clear them otherwise
 */
static int
replica_badblocks_check_or_clear(struct pool_set *set,
				struct poolset_health_status *set_hs,
				int dry_run, int called_from_sync,
				int check_bad_blocks, int fix_bad_blocks)
{
	LOG(3,
		"set %p, set_hs %p, dry_run %i, called_from_sync %i, "
		"check_bad_blocks %i, fix_bad_blocks %i",
		set, set_hs, dry_run, called_from_sync,
		check_bad_blocks, fix_bad_blocks);

#define ERR_MSG_BB \
	"       please read the manual first and use this option\n"\
	"       ONLY IF you are sure that you know what you are doing"

	enum badblocks_recovery_files_status status;
	int ret;

	/* check all bad block recovery files */
	status = replica_badblocks_recovery_files_check(set, set_hs);

	/* phase #1 - error handling */
	switch (status) {
	case RECOVERY_FILES_ERROR:
		LOG(1, "checking bad block recovery files failed");
		return -1;

	case RECOVERY_FILES_EXIST_ALL:
	case RECOVERY_FILES_NOT_ALL_EXIST:
		if (!called_from_sync) {
			ERR(
				"error: a bad block recovery file exists, run 'pmempool sync --bad-blocks' to fix bad blocks first");
			return -1;
		}

		if (!fix_bad_blocks) {
			ERR(
				"error: a bad block recovery file exists, but the '--bad-blocks' option is not set\n"
				ERR_MSG_BB);
			return -1;
		}
		break;

	default:
		break;
	};

	/*
	 * The pool is checked for bad blocks only if:
	 * 1) compat feature POOL_FEAT_CHECK_BAD_BLOCKS is set
	 *    OR:
	 * 2) the '--bad-blocks' option is set
	 *
	 * Bad blocks are cleared and fixed only if:
	 * - the '--bad-blocks' option is set
	 */
	if (!fix_bad_blocks && !check_bad_blocks) {
		LOG(3, "skipping bad blocks checking");
		return 0;
	}

	/* phase #2 - reading recovery files */
	switch (status) {
	case RECOVERY_FILES_EXIST_ALL:
		/* read all bad block recovery files */
		ret = replica_badblocks_recovery_files_read(set, set_hs);
		if (ret < 0) {
			LOG(1, "checking bad block recovery files failed");
			return -1;
		}

		if (ret > 0) {
			/* incomplete bad block recovery file was detected */

			LOG(1,
				"warning: incomplete bad block recovery file detected\n"
				"         - all recovery files will be removed");

			/* changing status to RECOVERY_FILES_NOT_ALL_EXIST */
			status = RECOVERY_FILES_NOT_ALL_EXIST;
		}
		break;

	case RECOVERY_FILES_NOT_ALL_EXIST:
		LOG(1,
			"warning: one of bad block recovery files does not exist\n"
			"         - all recovery files will be removed");
		break;

	default:
		break;
	};

	if (status == RECOVERY_FILES_NOT_ALL_EXIST) {
		/*
		 * At least one of bad block recovery files does not exist,
		 * or an incomplete bad block recovery file was detected,
		 * so all recovery files have to be removed.
		 */

		if (!dry_run) {
			LOG(1, "removing all bad block recovery files...");
			ret = replica_remove_all_recovery_files(set_hs);
			if (ret < 0) {
				LOG(1,
					"removing bad block recovery files failed");
				return -1;
			}
		} else {
			LOG(1, "all bad block recovery files would be removed");
		}

		/* changing status to RECOVERY_FILES_DO_NOT_EXIST */
		status = RECOVERY_FILES_DO_NOT_EXIST;
	}

	if (status == RECOVERY_FILES_DO_NOT_EXIST) {
		/*
		 * There are no bad block recovery files,
		 * so let's check bad blocks.
		 */

		int bad_blocks_found = replica_badblocks_get(set, set_hs);
		if (bad_blocks_found < 0) {
			if (errno == ENOTSUP) {
				LOG(1, BB_NOT_SUPP);
				return -1;
			}

			LOG(1, "checking bad blocks failed");
			return -1;
		}

		if (!bad_blocks_found) {
			LOG(4, "no bad blocks found");
			return 0;
		}

		/* bad blocks were found */

		if (!called_from_sync) {
			ERR(
				"error: bad blocks found, run 'pmempool sync --bad-blocks' to fix bad blocks first");
			return -1;
		}

		if (!fix_bad_blocks) {
			ERR(
				"error: bad blocks found, but the '--bad-blocks' option is not set\n"
				ERR_MSG_BB);
			return -1;
		}

		if (dry_run) {
			/* dry-run - do nothing */
			LOG(1, "warning: bad blocks were found");
			return 0;
		}

		/* create one empty recovery file for each part file */
		ret = replica_badblocks_recovery_files_create_empty(set,
								set_hs);
		if (ret < 0) {
			LOG(1,
				"creating empty bad block recovery files failed");
			return -1;
		}

		/* save bad blocks in recovery files */
		ret = replica_badblocks_recovery_files_save(set, set_hs);
		if (ret < 0) {
			LOG(1, "saving bad block recovery files failed");
			return -1;
		}
	}

	if (dry_run) {
		/* dry-run - do nothing */
		LOG(1, "bad blocks would be cleared");
		return 0;
	}

	ret = replica_badblocks_clear(set, set_hs);
	if (ret < 0) {
		ERR("clearing bad blocks failed");
		return -1;
	}

	return 0;
}

/*
 * check_shutdown_state -- (internal) check if replica has
 *			   healthy shutdown_state
 */
static int
check_shutdown_state(struct pool_set *set,
	struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, set_hs %p", set, set_hs);
	for (unsigned r = 0; r < set->nreplicas; ++r) {\
		struct pool_replica *rep = set->replica[r];
		struct replica_health_status *rep_hs = set_hs->replica[r];
		struct pool_hdr *hdrp = HDR(rep, 0);

		if (rep->remote)
			continue;

		if (hdrp == NULL) {
			/* cannot verify shutdown state */
			rep_hs->flags |= IS_BROKEN;
			continue;
		}

		struct shutdown_state curr_sds;
		shutdown_state_init(&curr_sds, NULL);
		for (unsigned p = 0; p < rep->nparts; ++p) {
			if (PART(rep, p)->fd < 0)
				continue;

			if (shutdown_state_add_part(&curr_sds,
					PART(rep, p)->fd, NULL)) {
				rep_hs->flags |= IS_BROKEN;
				break;
			}
		}

		if (rep_hs->flags & IS_BROKEN)
			continue;

		/* make a copy of sds as we shouldn't modify a pool */
		struct shutdown_state pool_sds = hdrp->sds;

		if (shutdown_state_check(&curr_sds, &pool_sds, NULL))
			rep_hs->flags |= IS_BROKEN;

	}
	return 0;
}

/*
 * check_uuids_between_parts -- (internal) check if uuids between adjacent
 *                              parts are consistent for a given replica
 */
static int
check_uuids_between_parts(struct pool_set *set, unsigned repn,
		struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, repn %u, set_hs %p", set, repn, set_hs);
	struct pool_replica *rep = REP(set, repn);

	/* check poolset_uuid consistency between replica's parts */
	LOG(4, "checking consistency of poolset uuid in replica %u", repn);
	uuid_t poolset_uuid;
	int uuid_stored = 0;
	unsigned part_stored = UNDEF_PART;
	for (unsigned p = 0; p < rep->nhdrs; ++p) {
		/* skip broken parts */
		if (replica_is_part_broken(repn, p, set_hs))
			continue;

		if (!uuid_stored) {
			memcpy(poolset_uuid, HDR(rep, p)->poolset_uuid,
					POOL_HDR_UUID_LEN);
			uuid_stored = 1;
			part_stored = p;
			continue;
		}

		if (uuidcmp(HDR(rep, p)->poolset_uuid, poolset_uuid)) {
			ERR(
				"different poolset uuids in parts from the same replica (repn %u, parts %u and %u) - cannot synchronize",
				repn, part_stored, p);
			errno = EINVAL;
			return -1;
		}
	}

	/* check if all uuids for adjacent replicas are the same across parts */
	LOG(4, "checking consistency of adjacent replicas' uuids in replica %u",
			repn);
	unsigned unbroken_p = UNDEF_PART;
	for (unsigned p = 0; p < rep->nhdrs; ++p) {
		/* skip broken parts */
		if (replica_is_part_broken(repn, p, set_hs))
			continue;

		if (unbroken_p == UNDEF_PART) {
			unbroken_p = p;
			continue;
		}

		struct pool_hdr *hdrp = HDR(rep, p);
		int prev_differ = uuidcmp(HDR(rep, unbroken_p)->prev_repl_uuid,
				hdrp->prev_repl_uuid);
		int next_differ = uuidcmp(HDR(rep, unbroken_p)->next_repl_uuid,
				hdrp->next_repl_uuid);

		if (prev_differ || next_differ) {
			ERR(
				"different adjacent replica UUID between parts (repn %u, parts %u and %u) - cannot synchronize",
				repn, unbroken_p, p);
			errno = EINVAL;
			return -1;
		}
	}

	/* check parts linkage */
	LOG(4, "checking parts linkage in replica %u", repn);
	for (unsigned p = 0; p < rep->nhdrs; ++p) {
		/* skip broken parts */
		if (replica_is_part_broken(repn, p, set_hs))
			continue;

		struct pool_hdr *hdrp = HDR(rep, p);
		struct pool_hdr *next_hdrp = HDRN(rep, p);
		int next_is_broken = replica_is_part_broken(repn, p + 1,
				set_hs);

		if (!next_is_broken) {
			int next_decoupled =
				uuidcmp(next_hdrp->prev_part_uuid,
					hdrp->uuid) ||
				uuidcmp(hdrp->next_part_uuid, next_hdrp->uuid);
			if (next_decoupled) {
				ERR(
					"two consecutive unbroken parts are not linked to each other (repn %u, parts %u and %u) - cannot synchronize",
					repn, p, p + 1);
				errno = EINVAL;
				return -1;
			}
		}
	}
	return 0;
}

/*
 * check_replicas_consistency -- (internal) check if all uuids within each
 *                               replica are consistent
 */
static int
check_replicas_consistency(struct pool_set *set,
		struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, set_hs %p", set, set_hs);
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		if (check_uuids_between_parts(set, r, set_hs))
			return -1;
	}
	return 0;
}

/*
 * check_replica_options -- (internal) check if options are consistent in the
 *                          replica
 */
static int
check_replica_options(struct pool_set *set, unsigned repn,
		struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, repn %u, set_hs %p", set, repn, set_hs);
	struct pool_replica *rep = REP(set, repn);
	struct replica_health_status *rep_hs = REP_HEALTH(set_hs, repn);
	for (unsigned p = 0; p < rep->nhdrs; ++p) {
		/* skip broken parts */
		if (replica_is_part_broken(repn, p, set_hs))
			continue;

		struct pool_hdr *hdr = HDR(rep, p);
		if (((hdr->features.incompat & POOL_FEAT_SINGLEHDR) == 0) !=
				((set->options & OPTION_SINGLEHDR) == 0)) {
			LOG(1,
				"improper options are set in part %u's header in replica %u",
				p, repn);
			rep_hs->part[p].flags |= IS_BROKEN;
		}
	}
	return 0;
}

/*
 * check_options -- (internal) check if options are consistent in all replicas
 */
static int
check_options(struct pool_set *set, struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, set_hs %p", set, set_hs);
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		if (check_replica_options(set, r, set_hs))
			return -1;
	}
	return 0;
}

/*
 * check_replica_poolset_uuids - (internal) check if poolset_uuid fields are
 *                               consistent among all parts of a replica;
 *                               the replica is initially considered as
 *                               consistent
 */
static int
check_replica_poolset_uuids(struct pool_set *set, unsigned repn,
		uuid_t poolset_uuid, struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, repn %u, poolset_uuid %p, set_hs %p", set, repn,
			poolset_uuid, set_hs);
	struct pool_replica *rep = REP(set, repn);
	for (unsigned p = 0; p < rep->nhdrs; ++p) {
		/* skip broken parts */
		if (replica_is_part_broken(repn, p, set_hs))
			continue;

		if (uuidcmp(HDR(rep, p)->poolset_uuid, poolset_uuid)) {
			/*
			 * two internally consistent replicas have
			 * different poolset_uuid
			 */
			return -1;
		} else {
			/*
			 * it is sufficient to check only one part
			 * from internally consistent replica
			 */
			break;
		}
	}
	return 0;
}

/*
 * check_poolset_uuids -- (internal) check if poolset_uuid fields are consistent
 *                        among all internally consistent replicas
 */
static int
check_poolset_uuids(struct pool_set *set,
		struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, set_hs %p", set, set_hs);

	/* find a replica with healthy header */
	unsigned r_h = replica_find_replica_healthy_header(set_hs);
	if (r_h == UNDEF_REPLICA) {
		ERR("no healthy replica found");
		return -1;
	}

	uuid_t poolset_uuid;
	memcpy(poolset_uuid, HDR(REP(set, r_h), 0)->poolset_uuid,
			POOL_HDR_UUID_LEN);

	for (unsigned r = 0; r < set->nreplicas; ++r) {
		/* skip inconsistent replicas */
		if (!replica_is_replica_consistent(r, set_hs) || r == r_h)
			continue;

		if (check_replica_poolset_uuids(set, r, poolset_uuid, set_hs)) {
			ERR(
				"inconsistent poolset uuids between replicas %u and %u - cannot synchronize",
				r_h, r);
			return -1;
		}
	}
	return 0;
}

/*
 * get_replica_uuid -- (internal) get replica uuid
 */
static int
get_replica_uuid(struct pool_replica *rep, unsigned repn,
		struct poolset_health_status *set_hs, uuid_t **uuidpp)
{
	unsigned nhdrs = rep->nhdrs;
	if (!replica_is_part_broken(repn, 0, set_hs)) {
		/* the first part is not broken */
		*uuidpp = &HDR(rep, 0)->uuid;
		return 0;
	} else if (nhdrs > 1 && !replica_is_part_broken(repn, 1, set_hs)) {
		/* the second part is not broken */
		*uuidpp = &HDR(rep, 1)->prev_part_uuid;
		return 0;
	} else if (nhdrs > 1 &&
			!replica_is_part_broken(repn, nhdrs - 1, set_hs)) {
		/* the last part is not broken */
		*uuidpp = &HDR(rep, nhdrs - 1)->next_part_uuid;
		return 0;
	} else {
		/* cannot get replica uuid */
		return -1;
	}
}

/*
 * check_uuids_between_replicas -- (internal) check if uuids between internally
 *                                 consistent adjacent replicas are consistent
 */
static int
check_uuids_between_replicas(struct pool_set *set,
		struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, set_hs %p", set, set_hs);
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		/* skip comparing inconsistent pairs of replicas */
		if (!replica_is_replica_consistent(r, set_hs) ||
				!replica_is_replica_consistent(r + 1, set_hs))
			continue;

		struct pool_replica *rep = REP(set, r);
		struct pool_replica *rep_n = REPN(set, r);

		/* get uuids of the two adjacent replicas */
		uuid_t *rep_uuidp = NULL;
		uuid_t *rep_n_uuidp = NULL;
		unsigned r_n = REPN_HEALTHidx(set_hs, r);
		if (get_replica_uuid(rep, r, set_hs, &rep_uuidp))
			LOG(2, "cannot get replica uuid, replica %u", r);
		if (get_replica_uuid(rep_n, r_n, set_hs, &rep_n_uuidp))
			LOG(2, "cannot get replica uuid, replica %u", r_n);

		/*
		 * check if replica uuids are consistent between two adjacent
		 * replicas
		 */
		unsigned p = replica_find_unbroken_part(r, set_hs);
		unsigned p_n = replica_find_unbroken_part(r_n, set_hs);
		if (p_n != UNDEF_PART && rep_uuidp != NULL &&
				uuidcmp(*rep_uuidp,
					HDR(rep_n, p_n)->prev_repl_uuid)) {
			ERR(
				"inconsistent replica uuids between replicas %u and %u",
				r, r_n);
			return -1;
		}
		if (p != UNDEF_PART && rep_n_uuidp != NULL &&
				uuidcmp(*rep_n_uuidp,
					HDR(rep, p)->next_repl_uuid)) {
			ERR(
				"inconsistent replica uuids between replicas %u and %u",
				r, r_n);
			return -1;
		}

		/*
		 * check if replica uuids on borders of a broken replica are
		 * consistent
		 */
		unsigned r_nn = REPN_HEALTHidx(set_hs, r_n);
		if (set->nreplicas > 1 && p != UNDEF_PART &&
				replica_is_replica_broken(r_n, set_hs) &&
				replica_is_replica_consistent(r_nn, set_hs)) {
			unsigned p_nn =
				replica_find_unbroken_part(r_nn, set_hs);
			if (p_nn == UNDEF_PART) {
				LOG(2,
					"cannot compare uuids on borders of replica %u",
					r);
				continue;
			}
			struct pool_replica *rep_nn = REP(set, r_nn);
			if (uuidcmp(HDR(rep, p)->next_repl_uuid,
					HDR(rep_nn, p_nn)->prev_repl_uuid)) {
				ERR(
					"inconsistent replica uuids on borders of replica %u",
					r);
				return -1;
			}
		}
	}
	return 0;
}

/*
 * check_replica_cycles -- (internal) check if healthy replicas form cycles
 *	shorter than the number of all replicas
 */
static int
check_replica_cycles(struct pool_set *set,
	struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, set_hs %p", set, set_hs);
	unsigned first_healthy;
	unsigned count_healthy = 0;
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		if (!replica_is_replica_healthy(r, set_hs)) {
			count_healthy = 0;
			continue;
		}

		if (count_healthy == 0)
			first_healthy = r;

		++count_healthy;
		struct pool_hdr *hdrh =
			PART(REP(set, first_healthy), 0)->hdr;
		struct pool_hdr *hdr = PART(REP(set, r), 0)->hdr;
		if (uuidcmp(hdrh->uuid, hdr->next_repl_uuid) == 0 &&
			count_healthy < set->nreplicas) {
			/*
			 * Healthy replicas form a cycle shorter than
			 * the number of all replicas; for the user it
			 * means that:
			 */
			ERR(
				"alien replica found (probably coming from a different poolset)");
			return -1;
		}
	}
	return 0;
}

/*
 * check_replica_sizes -- (internal) check if all replicas are large
 *	enough to hold data from a healthy replica
 */
static int
check_replica_sizes(struct pool_set *set, struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, set_hs %p", set, set_hs);
	ssize_t pool_size = -1;
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		/* skip broken replicas */
		if (!replica_is_replica_healthy(r, set_hs))
			continue;

		/* get the size of a pool in the replica */
		ssize_t replica_pool_size;
		if (REP(set, r)->remote)
			/* XXX: no way to get the size of a remote pool yet */
			replica_pool_size = (ssize_t)set->poolsize;
		else
			replica_pool_size = replica_get_pool_size(set, r);

		if (replica_pool_size < 0) {
			LOG(1, "getting pool size from replica %u failed", r);
			set_hs->replica[r]->flags |= IS_BROKEN;
			continue;
		}

		/* check if the pool is bigger than minimum size */
		enum pool_type type = pool_hdr_get_type(HDR(REP(set, r), 0));
		if ((size_t)replica_pool_size < pool_get_min_size(type)) {
			LOG(1,
				"pool size from replica %u is smaller than the minimum size allowed for the pool",
				r);
			set_hs->replica[r]->flags |= IS_BROKEN;
			continue;
		}

		/* check if each replica is big enough to hold the pool data */
		if (set->poolsize < (size_t)replica_pool_size) {
			ERR(
				"some replicas are too small to hold synchronized data");
			return -1;
		}

		if (pool_size < 0) {
			pool_size = replica_pool_size;
			continue;
		}

		/* check if pools in all healthy replicas are of equal size */
		if (pool_size != replica_pool_size) {
			ERR("pool sizes from different replicas differ");
			return -1;
		}
	}
	return 0;
}

/*
 * replica_read_features -- (internal) read features from the header
 */
static int
replica_read_features(struct pool_set *set,
			struct poolset_health_status *set_hs,
			features_t *features)
{
	LOG(3, "set %p set_hs %p features %p", set, set_hs, features);

	ASSERTne(features, NULL);

	for (unsigned r = 0; r < set->nreplicas; r++) {
		struct pool_replica *rep = set->replica[r];
		struct replica_health_status *rep_hs = set_hs->replica[r];

		if (rep->remote) {
			if (rep_hs->flags & IS_BROKEN)
				continue;

			struct pool_hdr *hdrp = rep->part[0].hdr;
			memcpy(features, &hdrp->features, sizeof(*features));

			return 0;
		}

		for (unsigned p = 0; p < rep->nparts; p++) {
			struct pool_set_part *part = &rep->part[p];

			if (part->fd == -1)
				continue;

			if (util_map_hdr(part, MAP_SHARED, 0) != 0) {
				LOG(1, "header mapping failed");
				return -1;
			}

			struct pool_hdr *hdrp = part->hdr;
			memcpy(features, &hdrp->features, sizeof(*features));

			util_unmap_hdr(part);

			return 0;
		}
	}

	/* no healthy replica/part found */
	return -1;
}

/*
 * replica_check_poolset_health -- check if a given poolset can be considered as
 *                         healthy, and store the status in a helping structure
 */
int
replica_check_poolset_health(struct pool_set *set,
		struct poolset_health_status **set_hsp,
		int called_from_sync, unsigned flags)
{
	LOG(3, "set %p, set_hsp %p, called_from_sync %i, flags %u",
		set, set_hsp, called_from_sync, flags);

	if (replica_create_poolset_health_status(set, set_hsp)) {
		LOG(1, "creating poolset health status failed");
		return -1;
	}

	struct poolset_health_status *set_hs = *set_hsp;

	/* check if part files exist and are accessible */
	if (check_and_open_poolset_part_files(set, set_hs, flags)) {
		LOG(1, "poolset part files check failed");
		goto err;
	}

	features_t features;
	int check_bad_blks;
	int fix_bad_blks = called_from_sync && fix_bad_blocks(flags);

	if (fix_bad_blks) {
		/*
		 * We will fix bad blocks, so we cannot read features here,
		 * because reading could fail, because of bad blocks.
		 * We will read features after having bad blocks fixed.
		 *
		 * Fixing bad blocks implies checking bad blocks.
		 */
		check_bad_blks = 1;
	} else {
		/*
		 * We will not fix bad blocks, so we have to read features here.
		 */
		if (replica_read_features(set, set_hs, &features)) {
			LOG(1, "reading features failed");
			goto err;
		}
		check_bad_blks = features.compat & POOL_FEAT_CHECK_BAD_BLOCKS;
	}

	/* check for bad blocks when in dry run or clear them otherwise */
	if (replica_badblocks_check_or_clear(set, set_hs, is_dry_run(flags),
			called_from_sync, check_bad_blks, fix_bad_blks)) {
		LOG(1, "replica bad_blocks check failed");
		goto err;
	}

	/* read features after fixing bad blocks */
	if (fix_bad_blks && replica_read_features(set, set_hs, &features)) {
		LOG(1, "reading features failed");
		goto err;
	}

	/* set ignore_sds flag basing on features read from the header */
	set->ignore_sds = !(features.incompat & POOL_FEAT_SDS);

	/* map all headers */
	map_all_unbroken_headers(set, set_hs);

	/*
	 * Check if checksums and signatures are correct for all parts
	 * in all replicas.
	 */
	check_checksums_and_signatures(set, set_hs);

	/* check if option flags are consistent */
	if (check_options(set, set_hs)) {
		LOG(1, "flags check failed");
		goto err;
	}

	if (!set->ignore_sds && check_shutdown_state(set, set_hs)) {
		LOG(1, "replica shutdown_state check failed");
		goto err;
	}

	/* check if uuids in parts across each replica are consistent */
	if (check_replicas_consistency(set, set_hs)) {
		LOG(1, "replica consistency check failed");
		goto err;
	}

	/* check poolset_uuid values between replicas */
	if (check_poolset_uuids(set, set_hs)) {
		LOG(1, "poolset uuids check failed");
		goto err;
	}

	/* check if uuids for adjacent replicas are consistent */
	if (check_uuids_between_replicas(set, set_hs)) {
		LOG(1, "replica uuids check failed");
		goto err;
	}

	/* check if healthy replicas make up another poolset */
	if (check_replica_cycles(set, set_hs)) {
		LOG(1, "replica cycles check failed");
		goto err;
	}

	/* check if replicas are large enough */
	if (check_replica_sizes(set, set_hs)) {
		LOG(1, "replica sizes check failed");
		goto err;
	}

	if (check_store_all_sizes(set, set_hs)) {
		LOG(1, "reading pool sizes failed");
		goto err;
	}

	unmap_all_headers(set);
	util_poolset_fdclose_always(set);
	return 0;

err:
	errno = EINVAL;
	unmap_all_headers(set);
	util_poolset_fdclose_always(set);
	replica_free_poolset_health_status(set_hs);
	return -1;
}

/*
 * replica_get_pool_size -- find the effective size (mapped) of a pool based
 *                          on metadata from given replica
 */
ssize_t
replica_get_pool_size(struct pool_set *set, unsigned repn)
{
	LOG(3, "set %p, repn %u", set, repn);
	struct pool_set_part *part = PART(REP(set, repn), 0);
	int should_close_part = 0;
	int should_unmap_part = 0;
	if (part->fd == -1) {
		if (util_part_open(part, 0, 0))
			return -1;

		should_close_part = 1;
	}

	if (part->addr == NULL) {
		if (util_map_part(part, NULL,
		    ALIGN_UP(sizeof(PMEMobjpool), part->alignment), 0,
		    MAP_SHARED, 1)) {
			util_part_fdclose(part);
			return -1;
		}
		should_unmap_part = 1;
	}

	PMEMobjpool *pop = (PMEMobjpool *)part->addr;
	ssize_t ret = (ssize_t)(pop->heap_offset + pop->heap_size);

	if (should_unmap_part)
		util_unmap_part(part);
	if (should_close_part)
		util_part_fdclose(part);

	return ret;
}

/*
 * replica_check_part_sizes -- check if all parts are large enough
 */
int
replica_check_part_sizes(struct pool_set *set, size_t min_size)
{
	LOG(3, "set %p, min_size %zu", set, min_size);
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = set->replica[r];
		if (rep->remote != NULL)
			/* skip remote replicas */
			continue;

		for (unsigned p = 0; p < rep->nparts; ++p) {
			if (PART(rep, p)->filesize < min_size) {
				ERR("replica %u, part %u: file is too small",
						r, p);
				errno = EINVAL;
				return -1;
			}
		}
	}
	return 0;
}

/*
 * replica_check_local_part_dir -- check if directory for the part file
 *                                 exists
 */
int
replica_check_local_part_dir(struct pool_set *set, unsigned repn,
		unsigned partn)
{
	LOG(3, "set %p, repn %u, partn %u", set, repn, partn);
	char *path = Strdup(PART(REP(set, repn), partn)->path);
	const char *dir = dirname(path);
	os_stat_t sb;
	if (os_stat(dir, &sb) != 0 || !(sb.st_mode & S_IFDIR)) {
		ERR(
			"directory %s for part %u in replica %u does not exist or is not accessible",
			path, partn, repn);
		Free(path);
		return -1;
	}
	Free(path);
	return 0;
}

/*
 * replica_check_part_dirs -- (internal) check if directories for part files
 *	exist
 */
int
replica_check_part_dirs(struct pool_set *set)
{
	LOG(3, "set %p", set);
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = set->replica[r];
		if (rep->remote != NULL)
			/* skip remote replicas */
			continue;

		for (unsigned p = 0; p < rep->nparts; ++p) {
			if (replica_check_local_part_dir(set, r, p))
				return -1;
		}
	}
	return 0;
}

/*
 * replica_open_replica_part_files -- open all part files for a replica
 */
int
replica_open_replica_part_files(struct pool_set *set, unsigned repn)
{
	LOG(3, "set %p, repn %u", set, repn);
	struct pool_replica *rep = set->replica[repn];
	for (unsigned p = 0; p < rep->nparts; ++p) {
		/* skip already opened files */
		if (rep->part[p].fd != -1)
			continue;

		if (util_part_open(&rep->part[p], 0, 0)) {
			LOG(1, "part files open failed for replica %u, part %u",
					repn, p);
			errno = EINVAL;
			goto err;
		}
	}
	return 0;

err:
	util_replica_fdclose(set->replica[repn]);
	return -1;
}

/*
 * replica_open_poolset_part_files -- open all part files for a poolset
 */
int
replica_open_poolset_part_files(struct pool_set *set)
{
	LOG(3, "set %p", set);
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		if (set->replica[r]->remote)
			continue;
		if (replica_open_replica_part_files(set, r)) {
			LOG(1, "opening replica %u, part files failed", r);
			goto err;
		}
	}

	return 0;

err:
	util_poolset_fdclose_always(set);
	return -1;
}

/*
 * pmempool_syncU -- synchronize replicas within a poolset
 */
#ifndef _WIN32
static inline
#endif
int
pmempool_syncU(const char *poolset, unsigned flags)
{
	LOG(3, "poolset %s, flags %u", poolset, flags);
	ASSERTne(poolset, NULL);

	/* check if poolset has correct signature */
	if (util_is_poolset_file(poolset) != 1) {
		ERR("file is not a poolset file");
		goto err;
	}

	/* check if flags are supported */
	if (check_flags_sync(flags)) {
		ERR("unsupported flags");
		errno = EINVAL;
		goto err;
	}

	/* open poolset file */
	int fd = util_file_open(poolset, NULL, 0, O_RDONLY);
	if (fd < 0) {
		ERR("cannot open a poolset file");
		goto err;
	}

	/* fill up pool_set structure */
	struct pool_set *set = NULL;
	if (util_poolset_parse(&set, poolset, fd)) {
		ERR("parsing input poolset failed");
		goto err_close_file;
	}

	if (set->nreplicas == 1) {
		ERR("no replica(s) found in the pool set");
		errno = EINVAL;
		goto err_close_file;
	}

	if (set->remote && util_remote_load()) {
		ERR("remote replication not available");
		errno = ENOTSUP;
		goto err_close_file;
	}

	/* sync all replicas */
	if (replica_sync(set, NULL, flags)) {
		LOG(1, "synchronization failed");
		goto err_close_all;
	}

	util_poolset_close(set, DO_NOT_DELETE_PARTS);
	os_close(fd);
	return 0;

err_close_all:
	util_poolset_close(set, DO_NOT_DELETE_PARTS);

err_close_file:
	os_close(fd);

err:
	if (errno == 0)
		errno = EINVAL;

	return -1;
}

#ifndef _WIN32
/*
 * pmempool_sync -- synchronize replicas within a poolset
 */
int
pmempool_sync(const char *poolset, unsigned flags)
{
	return pmempool_syncU(poolset, flags);
}
#else
/*
 * pmempool_syncW -- synchronize replicas within a poolset in widechar
 */
int
pmempool_syncW(const wchar_t *poolset, unsigned flags)
{
	char *path = util_toUTF8(poolset);
	if (path == NULL) {
		ERR("Invalid poolest file path.");
		return -1;
	}

	int ret = pmempool_syncU(path, flags);

	util_free_UTF8(path);
	return ret;
}
#endif

/*
 * pmempool_transformU -- alter poolset structure
 */
#ifndef _WIN32
static inline
#endif
int
pmempool_transformU(const char *poolset_src,
		const char *poolset_dst, unsigned flags)
{
	LOG(3, "poolset_src %s, poolset_dst %s, flags %u", poolset_src,
			poolset_dst, flags);
	ASSERTne(poolset_src, NULL);
	ASSERTne(poolset_dst, NULL);

	/* check if the source poolset has correct signature */
	if (util_is_poolset_file(poolset_src) != 1) {
		ERR("source file is not a poolset file");
		goto err;
	}

	/* check if the destination poolset has correct signature */
	if (util_is_poolset_file(poolset_dst) != 1) {
		ERR("destination file is not a poolset file");
		goto err;
	}

	/* check if flags are supported */
	if (check_flags_transform(flags)) {
		ERR("unsupported flags");
		errno = EINVAL;
		goto err;
	}

	/* open the source poolset file */
	int fd_in = util_file_open(poolset_src, NULL, 0, O_RDONLY);
	if (fd_in < 0) {
		ERR("cannot open source poolset file");
		goto err;
	}

	/* parse the source poolset file */
	struct pool_set *set_in = NULL;
	if (util_poolset_parse(&set_in, poolset_src, fd_in)) {
		ERR("parsing source poolset failed");
		os_close(fd_in);
		goto err;
	}
	os_close(fd_in);

	/* open the destination poolset file */
	int fd_out = util_file_open(poolset_dst, NULL, 0, O_RDONLY);
	if (fd_out < 0) {
		ERR("cannot open destination poolset file");
		goto err;
	}

	enum del_parts_mode del = DO_NOT_DELETE_PARTS;

	/* parse the destination poolset file */
	struct pool_set *set_out = NULL;
	if (util_poolset_parse(&set_out, poolset_dst, fd_out)) {
		ERR("parsing destination poolset failed");
		os_close(fd_out);
		goto err_free_poolin;
	}
	os_close(fd_out);

	/* check if the source poolset is of a correct type */
	enum pool_type ptype = pool_set_type(set_in);
	if (ptype != POOL_TYPE_OBJ) {
		errno = EINVAL;
		ERR("transform is not supported for given pool type: %s",
				pool_get_pool_type_str(ptype));
		goto err_free_poolout;
	}

	/* load remote library if needed */
	if (set_in->remote && util_remote_load()) {
		ERR("remote replication not available");
		goto err_free_poolout;
	}
	if (set_out->remote && util_remote_load()) {
		ERR("remote replication not available");
		goto err_free_poolout;
	}

	del = is_dry_run(flags) ? DO_NOT_DELETE_PARTS : DELETE_CREATED_PARTS;

	/* transform poolset */
	if (replica_transform(set_in, set_out, flags)) {
		LOG(1, "transformation failed");
		goto err_free_poolout;
	}

	util_poolset_close(set_in, DO_NOT_DELETE_PARTS);
	util_poolset_close(set_out, DO_NOT_DELETE_PARTS);
	return 0;

err_free_poolout:
	util_poolset_close(set_out, del);

err_free_poolin:
	util_poolset_close(set_in, DO_NOT_DELETE_PARTS);

err:
	if (errno == 0)
		errno = EINVAL;

	return -1;
}

#ifndef _WIN32
/*
 * pmempool_transform -- alter poolset structure
 */
int
pmempool_transform(const char *poolset_src,
	const char *poolset_dst, unsigned flags)
{
	return pmempool_transformU(poolset_src, poolset_dst, flags);
}
#else
/*
 * pmempool_transformW -- alter poolset structure in widechar
 */
int
pmempool_transformW(const wchar_t *poolset_src,
	const wchar_t *poolset_dst, unsigned flags)
{
	char *path_src = util_toUTF8(poolset_src);
	if (path_src == NULL) {
		ERR("Invalid source poolest file path.");
		return -1;
	}

	char *path_dst = util_toUTF8(poolset_dst);
	if (path_dst == NULL) {
		ERR("Invalid destination poolest file path.");
		Free(path_src);
		return -1;
	}

	int ret = pmempool_transformU(path_src, path_dst, flags);

	util_free_UTF8(path_src);
	util_free_UTF8(path_dst);
	return ret;
}
#endif
