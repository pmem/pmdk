// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2022, Intel Corporation */

/*
 * btt.c -- block translation table providing atomic block updates
 *
 * This is a user-space implementation of the BTT mechanism providing
 * single block powerfail write atomicity, as described by:
 *	The NVDIMM Namespace Specification
 *
 * To use this module, the caller must provide five routines for
 * accessing the namespace containing the data (in this context,
 * "namespace" refers to the storage containing the BTT layout, such
 * as a file).  All namespace I/O is done by these callbacks:
 *
 *	nsread	Read count bytes from namespace at offset off
 *	nswrite	Write count bytes to namespace at offset off
 *	nszero	Zero count bytes in namespace at offset off
 *	nsmap	Return direct access to a range of a namespace
 *	nssync	Flush changes made to an nsmap'd range
 *
 * Data written by the nswrite callback is flushed out to the media
 * (made durable) when the call returns.  Data written directly via
 * the nsmap callback must be flushed explicitly using nssync.
 *
 * The caller passes these callbacks, along with information such as
 * namespace size and UUID to btt_init() and gets back an opaque handle
 * which is then used with the rest of the entry points.
 *
 * Here is a brief list of the entry points to this module:
 *
 *	btt_nlane	Returns number of concurrent threads allowed
 *
 *	btt_nlba	Returns the usable size, as a count of LBAs
 *
 *	btt_read	Reads a single block at a given LBA
 *
 *	btt_write	Writes a single block (atomically) at a given LBA
 *
 *	btt_set_zero	Sets a block to read back as zeros
 *
 *	btt_set_error	Sets a block to return error on read
 *
 *	btt_check	Checks the BTT metadata for consistency
 *
 *	btt_fini	Frees run-time state, done using namespace
 *
 * If the caller is multi-threaded, it must only allow btt_nlane() threads
 * to enter this module at a time, each assigned a unique "lane" number
 * between 0 and btt_nlane() - 1.
 *
 * There are a number of static routines defined in this module.  Here's
 * a brief overview of the most important routines:
 *
 *	read_layout	Checks for valid BTT layout and builds run-time state.
 *			A number of helper functions are used by read_layout
 *			to handle various parts of the metadata:
 *				read_info
 *				read_arenas
 *				read_arena
 *				read_flogs
 *				read_flog_pair
 *
 *	write_layout	Generates a new BTT layout when one doesn't exist.
 *			Once a new layout is written, write_layout uses
 *			the same helper functions above to construct the
 *			run-time state.
 *
 *	invalid_lba	Range check done by each entry point that takes
 *			an LBA.
 *
 *	lba_to_arena_lba
 *			Find the arena and LBA in that arena for a given
 *			external LBA.  This is the heart of the arena
 *			range matching logic.
 *
 *	flog_update	Update the BTT free list/log combined data structure
 *			(known as the "flog").  This is the heart of the
 *			logic that makes writes powerfail atomic.
 *
 *	map_lock	These routines provide atomic access to the BTT map
 *	map_unlock	data structure in an area.
 *	map_abort
 *
 *	map_entry_setf	Common code for btt_set_zero() and btt_set_error().
 *
 *	zero_block	Generate a block of all zeros (instead of actually
 *			doing a read), when the metadata indicates the
 *			block should read as zeros.
 *
 *	build_rtt	These routines construct the run-time tracking
 *	build_map_locks	data structures used during I/O.
 */

#include <inttypes.h>
#include <stdio.h>
#include <sys/param.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <endian.h>

#include "out.h"
#include "uuid.h"
#include "btt.h"
#include "btt_layout.h"
#include "sys_util.h"
#include "util.h"
#include "alloc.h"

/*
 * The opaque btt handle containing state tracked by this module
 * for the btt namespace.  This is created by btt_init(), handed to
 * all the other btt_* entry points, and deleted by btt_fini().
 */
struct btt {
	unsigned nlane; /* number of concurrent threads allowed per btt */

	/*
	 * The laidout flag indicates whether the namespace contains valid BTT
	 * metadata.  It is initialized by read_layout() and if no valid layout
	 * is found, all reads return zeros and the first write will write the
	 * BTT layout.  The layout_write_mutex protects the laidout flag so
	 * only one write threads ends up writing the initial metadata by
	 * calling write_layout().
	 */
	os_mutex_t layout_write_mutex;
	int laidout;

	/*
	 * UUID of the BTT
	 */
	uint8_t uuid[BTTINFO_UUID_LEN];

	/*
	 * UUID of the containing namespace, used to validate BTT metadata.
	 */
	uint8_t parent_uuid[BTTINFO_UUID_LEN];

	/*
	 * Parameters controlling/describing the BTT layout.
	 */
	uint64_t rawsize;		/* size of containing namespace */
	uint32_t lbasize;		/* external LBA size */
	uint32_t nfree;			/* available flog entries */
	uint64_t nlba;			/* total number of external LBAs */
	unsigned narena;		/* number of arenas */

	/* run-time state kept for each arena */
	struct arena {
		uint32_t flags;		/* arena flags (btt_info) */
		uint32_t external_nlba;	/* LBAs that live in this arena */
		uint32_t internal_lbasize;
		uint32_t internal_nlba;
		uint16_t major;	/* major version, define the arena layout */

		/*
		 * The following offsets are relative to the beginning of
		 * the encapsulating namespace.  This is different from
		 * how these offsets are stored on-media, where they are
		 * relative to the start of the arena.  The offset are
		 * converted by read_layout() to make them more convenient
		 * for run-time use.
		 */
		uint64_t startoff;	/* offset to start of arena */
		uint64_t dataoff;	/* offset to arena data area */
		uint64_t mapoff;	/* offset to area map */
		uint64_t flogoff;	/* offset to area flog */
		uint64_t nextoff;	/* offset to next arena */

		/*
		 * Run-time flog state.  Indexed by lane.
		 *
		 * The write path uses the flog to find the free block
		 * it writes to before atomically making it the new
		 * active block for an external LBA.
		 *
		 * The read path doesn't use the flog at all.
		 */
		struct flog_runtime {
			struct btt_flog flog;	/* current info */
			uint64_t entries[2];	/* offsets for flog pair */
			int next;		/* next write (0 or 1) */
		} *flogs;

		/*
		 * Read tracking table.  Indexed by lane.
		 *
		 * Before using a free block found in the flog, the write path
		 * scans the rtt to see if there are any outstanding reads on
		 * that block (reads that started before the block was freed by
		 * a concurrent write).  Unused slots in the rtt are indicated
		 * by setting the error bit, BTT_MAP_ENTRY_ERROR, so that the
		 * entry won't match any post-map LBA when checked.
		 */
		uint32_t volatile *rtt;

		/*
		 * scan btt_map and generate sd_freelist in DRAM.
		 */
		struct free_list {
			uint32_t free_num;
			uint32_t *free_array;
		} sd_freelist;

		/* os_spinlock_t list_lock; */
		os_mutex_t list_lock;

		/*
		 * each lane, keep at least one free ABA
		 * if in the lane, no ABA, get one from freelist
		 */
		uint32_t *lane_free;

		/*
		 * Map locking.  Indexed by pre-map LBA modulo nlane.
		 */
		os_mutex_t *map_locks;

		/*
		 * Arena info block locking.
		 */
		os_mutex_t info_lock;
	} *arenas;

	/*
	 * Callbacks for doing I/O to namespace.  These are provided by
	 * the code calling the BTT module, which passes them in to
	 * btt_init().  All namespace I/O is done using these.
	 *
	 * The opaque namespace handle "ns" was provided by the code calling
	 * the BTT module and is passed to each callback to identify the
	 * namespace being accessed.
	 */
	void *ns;
	const struct ns_callback *ns_cbp;
};

/*
 * Signature for arena info blocks.  Total size is 16 bytes, including
 * the '\0' added to the string by the declaration (the last two bytes
 * of the string are '\0').
 */
static const char Sig[] = BTTINFO_SIG;

/*
 * Zeroed out flog entry, used when initializing the flog.
 */
static const struct btt_flog Zflog;

/*
 * Lookup table and macro for looking up sequence numbers.  These are
 * the 2-bit numbers that cycle between 01, 10, and 11.
 *
 * To advance a sequence number to the next number, use something like:
 *	seq = NSEQ(seq);
 */
static const unsigned Nseq[] = { 0, 2, 3, 1 };
#define NSEQ(seq) (Nseq[(seq) & 3])

/*
 * get_map_lock_num -- (internal) Calculate offset into map_locks[]
 *
 * map_locks[] contains nfree locks which are used to protect the map
 * from concurrent access to the same cache line.  The index into
 * map_locks[] is calculated by looking at the byte offset into the map
 * (premap_lba * BTT_MAP_ENTRY_SIZE), figuring out how many cache lines
 * that is into the map that is (dividing by BTT_MAP_LOCK_ALIGN), and
 * then selecting one of nfree locks (the modulo at the end).
 *
 * The extra cast is to keep gcc from generating a false positive
 * 64-32 bit conversion error when -fsanitize is set.
 */
static inline uint32_t
get_map_lock_num(uint32_t premap_lba, uint32_t nfree)
{
	return (uint32_t)(premap_lba * BTT_MAP_ENTRY_SIZE / BTT_MAP_LOCK_ALIGN)
		% nfree;
}

/*
 * invalid_lba -- (internal) set errno and return true if lba is invalid
 *
 * This function is used at the top of the entry points where an external
 * LBA is provided, like this:
 *
 *	if (invalid_lba(bttp, lba))
 *		return -1;
 */
static int
invalid_lba(struct btt *bttp, uint64_t lba)
{
	LOG(3, "bttp %p lba %" PRIu64, bttp, lba);

	if (lba >= bttp->nlba) {
		ERR("lba out of range (nlba %" PRIu64 ")", bttp->nlba);
		errno = EINVAL;
		return 1;
	}

	return 0;
}

/*
 * read_info -- (internal) convert btt_info to host byte order & validate
 *
 * Returns true if info block is valid, and all the integer fields are
 * converted to host byte order.  If the info block is not valid, this
 * routine returns false and the info block passed in is left in an
 * unknown state.
 */
static int
read_info(struct btt *bttp, struct btt_info *infop)
{
	LOG(3, "infop %p", infop);

	if (memcmp(infop->sig, Sig, BTTINFO_SIG_LEN)) {
		LOG(3, "signature invalid");
		return 0;
	}

	if (memcmp(infop->parent_uuid, bttp->parent_uuid, BTTINFO_UUID_LEN)) {
		LOG(3, "parent UUID mismatch");
		return 0;
	}

	/* to be valid, the fields must checksum correctly */
	if (!util_checksum(infop, sizeof(*infop), &infop->checksum, 0, 0)) {
		LOG(3, "invalid checksum");
		return 0;
	}

	/* to be valid, info block must have a major version of at least 1 */
	if ((infop->major = le16toh(infop->major)) == 0) {
		LOG(3, "invalid major version (0)");
		return 0;
	}

	infop->flags = le32toh(infop->flags);
	infop->minor = le16toh(infop->minor);
	infop->external_lbasize = le32toh(infop->external_lbasize);
	infop->external_nlba = le32toh(infop->external_nlba);
	infop->internal_lbasize = le32toh(infop->internal_lbasize);
	infop->internal_nlba = le32toh(infop->internal_nlba);
	infop->nfree = le32toh(infop->nfree);
	infop->infosize = le32toh(infop->infosize);
	infop->nextoff = le64toh(infop->nextoff);
	infop->dataoff = le64toh(infop->dataoff);
	infop->mapoff = le64toh(infop->mapoff);
	infop->flogoff = le64toh(infop->flogoff);
	infop->infooff = le64toh(infop->infooff);

	return 1;
}

/*
 * map_entry_is_zero -- (internal) checks if map_entry is in zero state
 */
static inline int
map_entry_is_zero(uint32_t map_entry)
{
	return (map_entry & ~BTT_MAP_ENTRY_LBA_MASK) == BTT_MAP_ENTRY_ZERO;
}

/*
 * map_entry_is_error -- (internal) checks if map_entry is in error state
 */
static inline int
map_entry_is_error(uint32_t map_entry)
{
	return (map_entry & ~BTT_MAP_ENTRY_LBA_MASK) == BTT_MAP_ENTRY_ERROR;
}

/*
 * map_entry_is_initial -- checks if map_entry is in initial state
 */
int
map_entry_is_initial(uint32_t map_entry)
{
	return (map_entry & ~BTT_MAP_ENTRY_LBA_MASK) == 0;
}

/*
 * map_entry_is_zero_or_initial -- (internal) checks if map_entry is in initial
 * or zero state
 */
static inline int
map_entry_is_zero_or_initial(uint32_t map_entry)
{
	uint32_t entry_flags = map_entry & ~BTT_MAP_ENTRY_LBA_MASK;
	return entry_flags == 0 || entry_flags == BTT_MAP_ENTRY_ZERO;
}

/*
 * btt_flog_get_valid -- return valid and current flog entry
 */
struct btt_flog *
btt_flog_get_valid(struct btt_flog *flog_pair, int *next)
{
	/*
	 * Interesting cases:
	 *	- no valid seq numbers:  layout consistency error
	 *	- one valid seq number:  that's the current entry
	 *	- two valid seq numbers: higher number is current entry
	 *	- identical seq numbers: layout consistency error
	 */
	if (flog_pair[0].seq == flog_pair[1].seq) {
		return NULL;
	} else if (flog_pair[0].seq == 0) {
		/* singleton valid flog at flog_pair[1] */
		*next = 0;
		return &flog_pair[1];
	} else if (flog_pair[1].seq == 0) {
		/* singleton valid flog at flog_pair[0] */
		*next = 1;
		return &flog_pair[0];
	} else if (NSEQ(flog_pair[0].seq) == flog_pair[1].seq) {
		/* flog_pair[1] has the later sequence number */
		*next = 0;
		return &flog_pair[1];
	} else {
		/* flog_pair[0] has the later sequence number */
		*next = 1;
		return &flog_pair[0];
	}
}

/*
 * read_flog_pair -- (internal) load up a single flog pair
 *
 * Zero is returned on success, otherwise -1/errno.
 */
static int
read_flog_pair(struct btt *bttp, unsigned lane, struct arena *arenap,
	uint64_t flog_off, struct flog_runtime *flog_runtimep, uint32_t flognum)
{
	LOG(5, "bttp %p lane %u arenap %p flog_off %" PRIu64 " runtimep %p "
		"flognum %u", bttp, lane, arenap, flog_off, flog_runtimep,
		flognum);

	flog_runtimep->entries[0] = flog_off;
	flog_runtimep->entries[1] = flog_off + sizeof(struct btt_flog);

	if (lane >= bttp->nfree) {
		ERR("invalid lane %u among nfree %d", lane, bttp->nfree);
		errno = EINVAL;
		return -1;
	}

	if (flog_off == 0) {
		ERR("invalid flog offset %" PRIu64, flog_off);
		errno = EINVAL;
		return -1;
	}

	struct btt_flog flog_pair[2];
	if ((*bttp->ns_cbp->nsread)(bttp->ns, lane, flog_pair,
				sizeof(flog_pair), flog_off) < 0)
		return -1;

	btt_flog_convert2h(&flog_pair[0]);
	if (invalid_lba(bttp, flog_pair[0].lba))
		return -1;

	btt_flog_convert2h(&flog_pair[1]);
	if (invalid_lba(bttp, flog_pair[1].lba))
		return -1;

	LOG(6, "flog_pair[0] flog_off %" PRIu64 " old_map %u new_map %u seq %u",
			flog_off, flog_pair[0].old_map,
			flog_pair[0].new_map, flog_pair[0].seq);
	LOG(6, "flog_pair[1] old_map %u new_map %u seq %u",
			flog_pair[1].old_map, flog_pair[1].new_map,
			flog_pair[1].seq);

	struct btt_flog *currentp = btt_flog_get_valid(flog_pair,
		&flog_runtimep->next);

	if (currentp == NULL) {
		ERR("flog layout error: bad seq numbers %d %d",
			flog_pair[0].seq, flog_pair[1].seq);
		arenap->flags |= BTTINFO_FLAG_ERROR;
		return 0;
	}

	LOG(6, "run-time flog next is %d", flog_runtimep->next);

	/* copy current flog into run-time flog state */
	flog_runtimep->flog = *currentp;

	LOG(9, "read flog[%u]: lba %u old %u%s%s%s new %u%s%s%s", flognum,
		currentp->lba,
		currentp->old_map & BTT_MAP_ENTRY_LBA_MASK,
		(map_entry_is_error(currentp->old_map)) ? " ERROR" : "",
		(map_entry_is_zero(currentp->old_map)) ? " ZERO" : "",
		(map_entry_is_initial(currentp->old_map)) ? " INIT" : "",
		currentp->new_map & BTT_MAP_ENTRY_LBA_MASK,
		(map_entry_is_error(currentp->new_map)) ? " ERROR" : "",
		(map_entry_is_zero(currentp->new_map)) ? " ZERO" : "",
		(map_entry_is_initial(currentp->new_map)) ? " INIT" : "");

	/*
	 * Decide if the current flog info represents a completed
	 * operation or an incomplete operation.  If completed, the
	 * old_map field will contain the free block to be used for
	 * the next write.  But if the operation didn't complete (indicated
	 * by the map entry not being updated), then the operation is
	 * completed now by updating the map entry.
	 *
	 * A special case, used by flog entries when first created, is
	 * when old_map == new_map.  This counts as a complete entry
	 * and doesn't require reading the map to see if recovery is
	 * required.
	 */
	if (currentp->old_map == currentp->new_map) {
		LOG(9, "flog[%u] entry complete (initial state)", flognum);
		return 0;
	}

	/* convert pre-map LBA into an offset into the map */
	uint64_t map_entry_off = arenap->mapoff +
				BTT_MAP_ENTRY_SIZE * currentp->lba;

	/* read current map entry */
	uint32_t entry;
	if ((*bttp->ns_cbp->nsread)(bttp->ns, lane, &entry,
				sizeof(entry), map_entry_off) < 0)
		return -1;

	entry = le32toh(entry);

	/* map entry in initial state */
	if (map_entry_is_initial(entry))
		entry = currentp->lba | BTT_MAP_ENTRY_NORMAL;

	if (currentp->new_map != entry && currentp->old_map == entry) {
		/* last update didn't complete */
		LOG(9, "recover flog[%u]: map[%u]: %u",
				flognum, currentp->lba, currentp->new_map);

		/*
		 * Recovery step is to complete the transaction by
		 * updating the map entry.
		 */
		entry = htole32(currentp->new_map);
		if ((*bttp->ns_cbp->nswrite)(bttp->ns, lane, &entry,
					sizeof(uint32_t), map_entry_off) < 0)
			return -1;
	}

	return 0;
}

/*
 * flog_update -- (internal) write out an updated flog entry
 *
 * The flog entries are not checksummed.  Instead, increasing sequence
 * numbers are used to atomically switch the active flog entry between
 * the first and second struct btt_flog in each slot.  In order for this
 * to work, the sequence number must be updated only after all the other
 * fields in the flog are updated.  So the writes to the flog are broken
 * into two writes, one for the first three fields (lba, old_map, new_map)
 * and, only after those fields are known to be written durably, the
 * second write for the seq field is done.
 *
 * Returns 0 on success, otherwise -1/errno.
 */
static int
flog_update(struct btt *bttp, unsigned lane, struct arena *arenap,
		uint32_t lba, uint32_t old_map, uint32_t new_map)
{
	LOG(3, "bttp %p lane %u arenap %p lba %u old_map %u new_map %u",
			bttp, lane, arenap, lba, old_map, new_map);

	/* construct new flog entry in little-endian byte order */
	struct btt_flog new_flog;
	new_flog.lba = lba;
	new_flog.old_map = old_map;
	new_flog.new_map = new_map;
	new_flog.seq = NSEQ(arenap->flogs[lane].flog.seq);
	btt_flog_convert2le(&new_flog);

	uint64_t new_flog_off =
		arenap->flogs[lane].entries[arenap->flogs[lane].next];

	/* write out first two fields first */
	if ((*bttp->ns_cbp->nswrite)(bttp->ns, lane, &new_flog,
				sizeof(uint32_t) * 2, new_flog_off) < 0)
		return -1;
	new_flog_off += sizeof(uint32_t) * 2;

	/* write out new_map and seq field to make it active */
	if ((*bttp->ns_cbp->nswrite)(bttp->ns, lane, &new_flog.new_map,
				sizeof(uint32_t) * 2, new_flog_off) < 0)
		return -1;

	/* flog entry written successfully, update run-time state */
	arenap->flogs[lane].next = 1 - arenap->flogs[lane].next;
	arenap->flogs[lane].flog.lba = lba;
	arenap->flogs[lane].flog.old_map = old_map;
	arenap->flogs[lane].flog.new_map = new_map;
	arenap->flogs[lane].flog.seq = NSEQ(arenap->flogs[lane].flog.seq);

	LOG(9, "update flog[%u]: lba %u old %u%s%s%s new %u%s%s%s", lane, lba,
			old_map & BTT_MAP_ENTRY_LBA_MASK,
			(map_entry_is_error(old_map)) ? " ERROR" : "",
			(map_entry_is_zero(old_map)) ? " ZERO" : "",
			(map_entry_is_initial(old_map)) ? " INIT" : "",
			new_map & BTT_MAP_ENTRY_LBA_MASK,
			(map_entry_is_error(new_map)) ? " ERROR" : "",
			(map_entry_is_zero(new_map)) ? " ZERO" : "",
			(map_entry_is_initial(new_map)) ? " INIT" : "");

	return 0;
}

/*
 * arena_setf -- (internal) updates the given flag for the arena info block
 */
static int
arena_setf(struct btt *bttp, struct arena *arenap, unsigned lane, uint32_t setf)
{
	LOG(3, "bttp %p arenap %p lane %u setf 0x%x", bttp, arenap, lane, setf);

	/* update runtime state */
	util_fetch_and_or32(&arenap->flags, setf);

	if (!bttp->laidout) {
		/* no layout yet to update */
		return 0;
	}

	/*
	 * Read, modify and write out the info block
	 * at both the beginning and end of the arena.
	 */
	uint64_t arena_off = arenap->startoff;

	struct btt_info info;

	/* protect from simultaneous writes to the layout */
	util_mutex_lock(&arenap->info_lock);

	if ((*bttp->ns_cbp->nsread)(bttp->ns, lane, &info,
			sizeof(info), arena_off) < 0) {
		goto err;
	}

	uint64_t infooff = le64toh(info.infooff);

	/* update flags */
	info.flags |= htole32(setf);

	/* update checksum */
	util_checksum(&info, sizeof(info), &info.checksum, 1, 0);

	if ((*bttp->ns_cbp->nswrite)(bttp->ns, lane, &info,
				sizeof(info), arena_off) < 0) {
		goto err;
	}

	if ((*bttp->ns_cbp->nswrite)(bttp->ns, lane, &info,
				sizeof(info), arena_off + infooff) < 0) {
		goto err;
	}

	util_mutex_unlock(&arenap->info_lock);
	return 0;

err:
	util_mutex_unlock(&arenap->info_lock);
	return -1;
}

/*
 * set_arena_error -- (internal) set the error flag for the given arena
 */
static int
set_arena_error(struct btt *bttp, struct arena *arenap, unsigned lane)
{
	LOG(3, "bttp %p arena %p lane %u", bttp, arenap, lane);

	return arena_setf(bttp, arenap, lane, BTTINFO_FLAG_ERROR);
}

/*
 * read_flogs -- (internal) load up all the flog entries for an arena
 *
 * Zero is returned on success, otherwise -1/errno.
 */
static int
read_flogs(struct btt *bttp, unsigned lane, struct arena *arenap)
{
	if ((arenap->flogs = Zalloc(bttp->nfree *
			sizeof(struct flog_runtime))) == NULL) {
		ERR("!Malloc for %u flog entries", bttp->nfree);
		return -1;
	}

	/*
	 * Load up the flog state.  read_flog_pair() will determine if
	 * any recovery steps are required take them on the in-memory
	 * data structures it creates. Sets error flag when it
	 * determines an invalid state.
	 */
	uint64_t flog_off = arenap->flogoff;
	struct flog_runtime *flog_runtimep = arenap->flogs;
	for (uint32_t i = 0; i < bttp->nfree; i++) {
		if (read_flog_pair(bttp, lane, arenap, flog_off,
						flog_runtimep, i) < 0) {
			set_arena_error(bttp, arenap, lane);
			return -1;
		}

		/* prepare for next time around the loop */
		flog_off += roundup(2 * sizeof(struct btt_flog),
				BTT_FLOG_PAIR_ALIGN);
		flog_runtimep++;
	}

	return 0;
}

/*
 * build_rtt -- (internal) construct a read tracking table for an arena
 *
 * Zero is returned on success, otherwise -1/errno.
 *
 * The rtt is big enough to hold an entry for each free block (nfree)
 * since nlane can't be bigger than nfree.  nlane may end up smaller,
 * in which case some of the high rtt entries will be unused.
 */
static int
build_rtt(struct btt *bttp, struct arena *arenap)
{
	if ((arenap->rtt = Malloc(bttp->nfree * sizeof(uint32_t)))
							== NULL) {
		ERR("!Malloc for %d rtt entries", bttp->nfree);
		return -1;
	}
	for (uint32_t lane = 0; lane < bttp->nfree; lane++)
		arenap->rtt[lane] = BTT_MAP_ENTRY_ERROR;
	util_synchronize();

	return 0;
}

/*
 * build_map_locks -- (internal) construct map locks
 *
 * Zero is returned on success, otherwise -1/errno.
 */
static int
build_map_locks(struct btt *bttp, struct arena *arenap)
{
	if ((arenap->map_locks =
			Malloc(bttp->nfree * sizeof(*arenap->map_locks)))
							== NULL) {
		ERR("!Malloc for %d map_lock entries", bttp->nfree);
		return -1;
	}
	for (uint32_t lane = 0; lane < bttp->nfree; lane++)
		util_mutex_init(&arenap->map_locks[lane]);

	return 0;
}

/*
 * get_lane_aba - get a free block out of the freelist.
 * @arena: arena handler
 * @lane: the block (postmap) will be put lane_free[lane]
 */
static inline void get_lane_aba(struct arena *arena,
		uint32_t lane, uint32_t *entry)
{
	uint32_t free_num;

	util_mutex_lock(&arena->list_lock);
	free_num = arena->sd_freelist.free_num;
	arena->lane_free[lane] =
		arena->sd_freelist.free_array[free_num - 1];
	arena->sd_freelist.free_num = free_num - 1;

	/*
	 * if free_num = 0, means all data block been written
	 * no operation on the freelist
	 * free the freelist.free_array
	 */
	if (arena->sd_freelist.free_num == 0) {
		Free(arena->sd_freelist.free_array);
		arena->sd_freelist.free_array = NULL;
	}

	util_mutex_unlock(&arena->list_lock);
	*entry = arena->lane_free[lane];
}

static int btt_map_read(struct btt *bttp, struct arena *arena,
	uint32_t lane, uint32_t lba, uint32_t *mapping)
{
	/* convert pre-map LBA into an offset into the map */
	uint64_t map_entry_off = arena->mapoff + BTT_MAP_ENTRY_SIZE * lba;

	/*
	 * Read the current map entry to get the post-map LBA for the data
	 * block read.
	 */
	uint32_t entry;

	if ((*bttp->ns_cbp->nsread)(bttp->ns, lane, &entry,
		sizeof(entry), map_entry_off) < 0)
		return -1;

	entry = le32toh(entry);
	*mapping = entry;
	return 0;
}

static int btt_freelist_init(struct btt *bttp, struct arena *arena)
{
	int ret;
	uint32_t i;
	uint32_t mapping;
	uint8_t *aba_map_byte, *aba_map;
	uint32_t *free_array;
	uint32_t free_num = 0;

#ifdef DEBUG
	uint32_t init_s = 0;
	uint32_t other_s = 0;
#endif

	uint32_t aba_map_size = (arena->internal_nlba>>3) + 1;
	aba_map = Zalloc(aba_map_size);
	if ((!aba_map)) {
		ERR("!Malloc for aba_map size = %d \n", aba_map_size);
		return -1;
	}

	/*
	 * prepare the aba_map, each aba will be in a bit.
	 * occupied bit=1, free bit=0.
	 * the scan will take times, once execution during initilization.
	 */
	for (i = 0; i < arena->external_nlba; i++) {
		ret = btt_map_read(bttp, arena, 0, i, &mapping);
#ifdef DEBUG
		if (map_entry_is_initial(mapping)) init_s++;
#endif
		if (ret || map_entry_is_initial(mapping)) {
			continue;
		}
		mapping = mapping & BTT_MAP_ENTRY_LBA_MASK;
		if (mapping < arena->internal_nlba) {
			aba_map_byte = aba_map + (mapping>>3);
			*aba_map_byte |= (uint8_t)(1<<(mapping % 8));
		} else {
			LOG(9, "%s: mapping %#x out of range ",
				__func__, mapping);
		}
	}

	/*
	 * Scan the aba_bitmap , use the static array, that will take 1% memory.
	 */
	free_array = Malloc((uint32_t)(arena->internal_nlba *
					sizeof(uint32_t)));
	if (!free_array) {
		ERR("!Malloc for free_array size = %ld ",
			arena->internal_nlba * sizeof(uint32_t));
		Free(aba_map);
		return -1;
	}

	for (i = 0; i < arena->internal_nlba; i++) {
		aba_map_byte = aba_map + (i>>3);
		if (((*aba_map_byte) & (1<< (i % 8))) == 0) {
				free_array[free_num] = i;
				free_num++;
		}
	}
	ASSERT(free_num >= bttp->nfree);

	util_mutex_init(&arena->list_lock);

	arena->lane_free = Malloc((uint32_t)bttp->nfree *
				sizeof(uint32_t));
	if (!arena->lane_free) {
		ERR("!Malloc for lane_free size = %ld ",
				bttp->nfree * sizeof(uint32_t));
		Free(aba_map);
		Free(free_array);
		return -1;
	}

	for (i = 0; i < bttp->nfree; i++) {
		arena->lane_free[i] = free_array[free_num - 1];
		free_num--;
	}

#ifdef DEBUG
	LOG(9, "%s: free_num=%d, init_s=%d ",
			__func__, free_num, init_s);
#endif
	/*
	 * if free_num = 0, means all data block been written
	 * no operation on the freelist
	 * free the freelist.free_array
	 */
	if (free_num == 0) {
		Free(free_array);
		arena->sd_freelist.free_array = NULL;
		arena->sd_freelist.free_num = free_num;
	} else {
		arena->sd_freelist.free_array = free_array;
		arena->sd_freelist.free_num = free_num;
	}

	Free(aba_map);
	return 0;
}

/*
 * read_arena -- (internal) load up an arena and build run-time state
 *
 * Zero is returned on success, otherwise -1/errno.
 */
static int
read_arena(struct btt *bttp, unsigned lane, uint64_t arena_off,
		struct arena *arenap)
{
	LOG(3, "bttp %p lane %u arena_off %" PRIu64 " arenap %p",
			bttp, lane, arena_off, arenap);

	struct btt_info info;
	if ((*bttp->ns_cbp->nsread)(bttp->ns, lane, &info, sizeof(info),
							arena_off) < 0)
		return -1;

	arenap->flags = le32toh(info.flags);
	arenap->external_nlba = le32toh(info.external_nlba);
	arenap->internal_lbasize = le32toh(info.internal_lbasize);
	arenap->internal_nlba = le32toh(info.internal_nlba);
	arenap->major = le16toh(info.major);

	arenap->startoff = arena_off;
	arenap->dataoff = arena_off + le64toh(info.dataoff);
	arenap->mapoff = arena_off + le64toh(info.mapoff);
	arenap->nextoff = arena_off + le64toh(info.nextoff);
	arenap->flogoff = arena_off + le64toh(info.flogoff);

	if (arenap->major == 1) {
		if (read_flogs(bttp, lane, arenap) < 0)
			return -1;
	} else {
		if (btt_freelist_init(bttp, arenap) < 0)
			return -1;
	}

	if (build_rtt(bttp, arenap) < 0)
		return -1;

	if (build_map_locks(bttp, arenap) < 0)
		return -1;

	/* initialize the per arena info block lock */
	util_mutex_init(&arenap->info_lock);

	return 0;
}

/*
 * util_convert2h_btt_info -- convert btt_info to host byte order
 */
void
btt_info_convert2h(struct btt_info *infop)
{
	infop->flags = le32toh(infop->flags);
	infop->major = le16toh(infop->major);
	infop->minor = le16toh(infop->minor);
	infop->external_lbasize = le32toh(infop->external_lbasize);
	infop->external_nlba = le32toh(infop->external_nlba);
	infop->internal_lbasize = le32toh(infop->internal_lbasize);
	infop->internal_nlba = le32toh(infop->internal_nlba);
	infop->nfree = le32toh(infop->nfree);
	infop->infosize = le32toh(infop->infosize);
	infop->nextoff = le64toh(infop->nextoff);
	infop->dataoff = le64toh(infop->dataoff);
	infop->mapoff = le64toh(infop->mapoff);
	infop->flogoff = le64toh(infop->flogoff);
	infop->infooff = le64toh(infop->infooff);
}

/*
 * btt_info_convert2le -- convert btt_info to little-endian byte order
 */
void
btt_info_convert2le(struct btt_info *infop)
{
	infop->flags = le32toh(infop->flags);
	infop->major = le16toh(infop->major);
	infop->minor = le16toh(infop->minor);
	infop->external_lbasize = le32toh(infop->external_lbasize);
	infop->external_nlba = le32toh(infop->external_nlba);
	infop->internal_lbasize = le32toh(infop->internal_lbasize);
	infop->internal_nlba = le32toh(infop->internal_nlba);
	infop->nfree = le32toh(infop->nfree);
	infop->infosize = le32toh(infop->infosize);
	infop->nextoff = le64toh(infop->nextoff);
	infop->dataoff = le64toh(infop->dataoff);
	infop->mapoff = le64toh(infop->mapoff);
	infop->flogoff = le64toh(infop->flogoff);
	infop->infooff = le64toh(infop->infooff);
}

/*
 * btt_flog_convert2h -- convert btt_flog to host byte order
 */
void
btt_flog_convert2h(struct btt_flog *flogp)
{
	flogp->lba = le32toh(flogp->lba);
	flogp->old_map = le32toh(flogp->old_map);
	flogp->new_map = le32toh(flogp->new_map);
	flogp->seq = le32toh(flogp->seq);
}

/*
 * btt_flog_convert2le -- convert btt_flog to LE byte order
 */
void
btt_flog_convert2le(struct btt_flog *flogp)
{
	flogp->lba = htole32(flogp->lba);
	flogp->old_map = htole32(flogp->old_map);
	flogp->new_map = htole32(flogp->new_map);
	flogp->seq = htole32(flogp->seq);
}

/*
 * read_arenas -- (internal) load up all arenas and build run-time state
 *
 * On entry, layout must be known to be valid, and the number of arenas
 * must be known.  Zero is returned on success, otherwise -1/errno.
 */
static int
read_arenas(struct btt *bttp, unsigned lane, unsigned narena)
{
	LOG(3, "bttp %p lane %u narena %d", bttp, lane, narena);

	if ((bttp->arenas = Zalloc(narena * sizeof(*bttp->arenas))) == NULL) {
		ERR("!Malloc for %u arenas", narena);
		goto err;
	}

	uint64_t arena_off = 0;
	struct arena *arenap = bttp->arenas;
	for (unsigned i = 0; i < narena; i++) {

		if (read_arena(bttp, lane, arena_off, arenap) < 0)
			goto err;

		/* prepare for next time around the loop */
		arena_off = arenap->nextoff;
		arenap++;
	}

	bttp->laidout = 1;

	return 0;

err:
	LOG(4, "error clean up");
	int oerrno = errno;
	uint32_t *free_data;
	if (bttp->arenas) {
		for (unsigned i = 0; i < bttp->narena; i++) {
			if (bttp->arenas[i].major == 1 &&
				bttp->arenas[i].flogs) {
				Free(bttp->arenas[i].flogs);
			} else {
				free_data =
					bttp->arenas[i].sd_freelist.free_array;
				if (free_data)
					Free((void *)free_data);
				free_data = bttp->arenas[i].lane_free;
				if (free_data)
					Free((void *)free_data);
			}
			if (bttp->arenas[i].rtt)
				Free((void *)bttp->arenas[i].rtt);
			if (bttp->arenas[i].map_locks)
				Free((void *)bttp->arenas[i].map_locks);
		}
		Free(bttp->arenas);
		bttp->arenas = NULL;
	}
	errno = oerrno;
	return -1;
}

/*
 * internal_lbasize -- (internal) calculate internal LBA size
 */
static inline uint32_t
internal_lbasize(uint32_t external_lbasize)
{
	uint32_t internal_lbasize = external_lbasize;
	if (internal_lbasize < BTT_MIN_LBA_SIZE)
		internal_lbasize = BTT_MIN_LBA_SIZE;
	internal_lbasize =
		roundup(internal_lbasize, BTT_INTERNAL_LBA_ALIGNMENT);
	/* check for overflow */
	if (internal_lbasize < BTT_INTERNAL_LBA_ALIGNMENT) {
		errno = EINVAL;
		ERR("!Invalid lba size after alignment: %u ", internal_lbasize);
		return 0;
	}

	return internal_lbasize;
}

/*
 * btt_flog_size -- calculate flog data size
 */
uint64_t
btt_flog_size(uint32_t nfree)
{
	uint64_t flog_size = nfree * roundup(2 * sizeof(struct btt_flog),
		BTT_FLOG_PAIR_ALIGN);
	return roundup(flog_size, BTT_ALIGNMENT);
}

/*
 * btt_map_size -- calculate map data size
 */
uint64_t
btt_map_size(uint32_t external_nlba)
{
	return roundup(external_nlba * BTT_MAP_ENTRY_SIZE, BTT_ALIGNMENT);
}

/*
 * btt_arena_datasize -- whole arena size without BTT Info header, backup and
 *	flog means size of blocks and map
 */
uint64_t
btt_arena_datasize(uint64_t arena_size, uint32_t nfree)
{
	return arena_size - 2 * sizeof(struct btt_info) - btt_flog_size(nfree);
}

/*
 * btt_info_set_params -- (internal) calculate and set BTT Info
 *	external_lbasize, internal_lbasize, nfree, infosize, external_nlba and
 *	internal_nlba
 */
static int
btt_info_set_params(struct btt_info *info, uint32_t external_lbasize,
	uint32_t internal_lbasize, uint32_t nfree, uint64_t arena_size)
{
	info->external_lbasize = external_lbasize;
	info->internal_lbasize = internal_lbasize;
	info->nfree = nfree;
	info->infosize = sizeof(*info);

	uint64_t arena_data_size = btt_arena_datasize(arena_size, nfree);

	/* allow for map alignment padding */
	uint64_t internal_nlba = (arena_data_size - BTT_ALIGNMENT) /
		(info->internal_lbasize + BTT_MAP_ENTRY_SIZE);

	/* ensure the number of blocks is at least 2*nfree */
	if (internal_nlba < 2 * nfree) {
		errno = EINVAL;
		ERR("!number of internal blocks: %" PRIu64
			" expected at least %u",
			internal_nlba, 2 * nfree);
		return -1;
	}

	ASSERT(internal_nlba <= UINT32_MAX);
	uint32_t internal_nlba_u32 = (uint32_t)internal_nlba;

	info->internal_nlba = internal_nlba_u32;
	/* external LBA does not include free blocks */
	info->external_nlba = internal_nlba_u32 - info->nfree;

	ASSERT((arena_data_size - btt_map_size(info->external_nlba)) /
		internal_lbasize >= internal_nlba);

	return 0;
}

/*
 * btt_info_set_offs -- (internal) calculate and set the BTT Info dataoff,
 *	nextoff, infooff, flogoff and mapoff. These are all relative to the
 *	beginning of the arena.
 */
static void
btt_info_set_offs(struct btt_info *info, uint64_t arena_size,
	uint64_t space_left)
{
	info->dataoff = info->infosize;

	/* set offset to next valid arena */
	if (space_left >= BTT_MIN_SIZE)
		info->nextoff = arena_size;
	else
		info->nextoff = 0;

	info->infooff = arena_size - sizeof(struct btt_info);
	info->flogoff = info->infooff - btt_flog_size(info->nfree);
	info->mapoff = info->flogoff - btt_map_size(info->external_nlba);

	ASSERTeq(btt_arena_datasize(arena_size, info->nfree) -
		btt_map_size(info->external_nlba), info->mapoff -
		info->dataoff);
}

/*
 * btt_info_set -- set BTT Info params and offsets
 */
int
btt_info_set(struct btt_info *info, uint32_t external_lbasize,
	uint32_t nfree, uint64_t arena_size, uint64_t space_left)
{
	/* calculate internal LBA size */
	uint32_t internal_lba_size = internal_lbasize(external_lbasize);
	if (internal_lba_size == 0)
		return -1;

	/* set params and offsets */
	if (btt_info_set_params(info, external_lbasize,
			internal_lba_size, nfree, arena_size))
		return -1;

	btt_info_set_offs(info, arena_size, space_left);

	return 0;
}

/*
 * write_layout -- (internal) write out the initial btt metadata layout
 *
 * Called with write == 1 only once in the life time of a btt namespace, when
 * the first write happens.  The caller of this routine is responsible for
 * locking out multiple threads.  This routine doesn't read anything -- by the
 * time it is called, it is known there's no layout in the namespace and a new
 * layout should be written.
 *
 * Calling with write == 0 tells this routine to do the calculations for
 * bttp->narena and bttp->nlba, but don't write out any metadata.
 *
 * If successful, sets bttp->layout to 1 and returns 0.  Otherwise -1
 * is returned and errno is set, and bttp->layout remains 0 so that
 * later attempts to write will try again to create the layout.
 */
static int
write_layout(struct btt *bttp, unsigned lane, int write)
{
	LOG(3, "bttp %p lane %u write %d", bttp, lane, write);

	ASSERT(bttp->rawsize >= BTT_MIN_SIZE);
	ASSERT(bttp->nfree);

	/*
	 * If a new layout is being written, generate the BTT's UUID.
	 */
	if (write) {
		int ret = util_uuid_generate(bttp->uuid);
		if (ret < 0) {
			LOG(2, "util_uuid_generate failed");
			return -1;
		}
	}

	/*
	 * The number of arenas is the number of full arena of
	 * size BTT_MAX_ARENA that fit into rawsize and then, if
	 * the remainder is at least BTT_MIN_SIZE in size, then
	 * that adds one more arena.
	 */
	bttp->narena = (unsigned)(bttp->rawsize / BTT_MAX_ARENA);
	if (bttp->rawsize % BTT_MAX_ARENA >= BTT_MIN_SIZE)
		bttp->narena++;
	LOG(4, "narena %u", bttp->narena);

	uint32_t internal_lba_size = internal_lbasize(bttp->lbasize);
	if (internal_lba_size == 0)
		return -1;
	LOG(4, "adjusted internal_lbasize %u", internal_lba_size);

	uint64_t total_nlba = 0;
	uint64_t rawsize = bttp->rawsize;
	unsigned arena_num = 0;
	uint64_t arena_off = 0;

	/*
	 * for each arena...
	 */
	while (rawsize >= BTT_MIN_SIZE) {
		LOG(4, "layout arena %u", arena_num);

		uint64_t arena_rawsize = rawsize;
		if (arena_rawsize > BTT_MAX_ARENA) {
			arena_rawsize = BTT_MAX_ARENA;
		}
		rawsize -= arena_rawsize;
		arena_num++;

		struct btt_info info;
		memset(&info, '\0', sizeof(info));

		/*
		 * Construct the BTT info block and write it out
		 * at both the beginning and end of the arena.
		 */
		memcpy(info.sig, Sig, BTTINFO_SIG_LEN);
		memcpy(info.uuid, bttp->uuid, BTTINFO_UUID_LEN);
		memcpy(info.parent_uuid, bttp->parent_uuid, BTTINFO_UUID_LEN);
		info.major = BTTINFO_MAJOR_VERSION;
		info.minor = BTTINFO_MINOR_VERSION;

		if (btt_info_set_params(&info, bttp->lbasize,
				internal_lba_size, bttp->nfree, arena_rawsize))
			return -1;

		LOG(4, "internal_nlba %u external_nlba %u",
			info.internal_nlba, info.external_nlba);

		total_nlba += info.external_nlba;

		/*
		 * The rest of the loop body calculates metadata structures
		 * and lays it out for this arena.  So only continue if
		 * the write flag is set.
		 */
		if (!write)
			continue;

		btt_info_set_offs(&info, arena_rawsize, rawsize);

		LOG(4, "nextoff 0x%016" PRIx64, info.nextoff);
		LOG(4, "dataoff 0x%016" PRIx64, info.dataoff);
		LOG(4, "mapoff  0x%016" PRIx64, info.mapoff);
		LOG(4, "flogoff 0x%016" PRIx64, info.flogoff);
		LOG(4, "infooff 0x%016" PRIx64, info.infooff);

		/* zero map if ns is not zero-initialized */
		if (!bttp->ns_cbp->ns_is_zeroed) {
			uint64_t mapsize = btt_map_size(info.external_nlba);
			if ((*bttp->ns_cbp->nszero)(bttp->ns, lane, mapsize,
					info.mapoff) < 0)
				return -1;
		}

		if (info.major == 1) {
			/* write out the initial flog */
			uint64_t flog_entry_off = arena_off + info.flogoff;
			uint32_t next_free_lba = info.external_nlba;
			for (uint32_t i = 0; i < bttp->nfree; i++) {
				struct btt_flog flog;
				flog.lba = htole32(i);
				flog.old_map = flog.new_map =
					htole32(next_free_lba |
					BTT_MAP_ENTRY_ZERO);
				flog.seq = htole32(1);

				/*
				 * Write both btt_flog structs in the pair,
				 * writing the second one as all zeros.
				 */
				LOG(6, "flog[%u] entry off %" PRIu64
					" initial %u + zero = %u",
					i, flog_entry_off,
					next_free_lba,
					next_free_lba | BTT_MAP_ENTRY_ZERO);
				if ((*bttp->ns_cbp->nswrite)(bttp->ns,
					lane, &flog, sizeof(flog),
					flog_entry_off) < 0)
					return -1;
				flog_entry_off += sizeof(flog);

				LOG(6, "flog[%u] entry off %" PRIu64 " zeros",
						i, flog_entry_off);
				if ((*bttp->ns_cbp->nswrite)(bttp->ns,
					lane, &Zflog,
					sizeof(Zflog), flog_entry_off) < 0)
					return -1;
				flog_entry_off += sizeof(flog);
				flog_entry_off = roundup(flog_entry_off,
						BTT_FLOG_PAIR_ALIGN);

				next_free_lba++;
			}
		}

		btt_info_convert2le(&info);

		util_checksum(&info, sizeof(info), &info.checksum, 1, 0);

		if ((*bttp->ns_cbp->nswrite)(bttp->ns, lane, &info,
				sizeof(info), arena_off) < 0)
			return -1;
		if ((*bttp->ns_cbp->nswrite)(bttp->ns, lane, &info,
				sizeof(info), arena_off + info.infooff) < 0)
			return -1;

		arena_off += info.nextoff;
	}

	ASSERTeq(bttp->narena, arena_num);

	bttp->nlba = total_nlba;

	if (write) {
		/*
		 * The layout is written now, so load up the arenas.
		 */
		return read_arenas(bttp, lane, bttp->narena);
	}

	return 0;
}

/*
 * read_layout -- (internal) load up layout info from btt namespace
 *
 * Called once when the btt namespace is opened for use.
 * Sets bttp->layout to 0 if no valid layout is found, 1 otherwise.
 *
 * Any recovery actions required (as indicated by the flog state) are
 * performed by this routine.
 *
 * Any quick checks for layout consistency are performed by this routine
 * (quick enough to be done each time a BTT area is opened for use, not
 * like the slow consistency checks done by btt_check()).
 *
 * Returns 0 if no errors are encountered accessing the namespace (in this
 * context, detecting there's no layout is not an error if the nsread function
 * didn't have any problems doing the reads).  Otherwise, -1 is returned
 * and errno is set.
 */
static int
read_layout(struct btt *bttp, unsigned lane)
{
	LOG(3, "bttp %p", bttp);

	ASSERT(bttp->rawsize >= BTT_MIN_SIZE);

	unsigned narena = 0;
	uint32_t smallest_nfree = UINT32_MAX;
	uint64_t rawsize = bttp->rawsize;
	uint64_t total_nlba = 0;
	uint64_t arena_off = 0;

	bttp->nfree = BTT_DEFAULT_NFREE;

	/*
	 * For each arena, see if there's a valid info block
	 */
	while (rawsize >= BTT_MIN_SIZE) {
		narena++;

		struct btt_info info;
		if ((*bttp->ns_cbp->nsread)(bttp->ns, lane, &info,
					sizeof(info), arena_off) < 0)
			return -1;

		if (!read_info(bttp, &info)) {
			/*
			 * Failed to find complete BTT metadata.  Just
			 * calculate the narena and nlba values that will
			 * result when write_layout() gets called.  This
			 * allows checks against nlba to work correctly
			 * even before the layout is written.
			 */
			return write_layout(bttp, lane, 0);
		}
		if (info.external_lbasize != bttp->lbasize) {
			/* can't read it assuming the wrong block size */
			ERR("inconsistent lbasize");
			errno = EINVAL;
			return -1;
		}

		if (info.nfree == 0) {
			ERR("invalid nfree");
			errno = EINVAL;
			return -1;
		}

		if (info.external_nlba == 0) {
			ERR("invalid external_nlba");
			errno = EINVAL;
			return -1;
		}

		if (info.nextoff && (info.nextoff != BTT_MAX_ARENA)) {
			ERR("invalid arena size");
			errno = EINVAL;
			return -1;
		}

		if (info.nfree < smallest_nfree)
			smallest_nfree = info.nfree;

		total_nlba += info.external_nlba;
		arena_off += info.nextoff;
		if (info.nextoff == 0)
			break;
		if (info.nextoff > rawsize) {
			ERR("invalid next arena offset");
			errno = EINVAL;
			return -1;
		}
		rawsize -= info.nextoff;
	}

	ASSERT(narena);

	bttp->narena = narena;
	bttp->nlba = total_nlba;

	/*
	 * All arenas were valid.  nfree should be the smallest value found
	 * among different arenas.
	 */
	if (smallest_nfree < bttp->nfree)
		bttp->nfree = smallest_nfree;

	/*
	 * Load up arenas.
	 */
	return read_arenas(bttp, lane, narena);
}

/*
 * zero_block -- (internal) satisfy a read with a block of zeros
 *
 * Returns 0 on success, otherwise -1/errno.
 */
static int
zero_block(struct btt *bttp, void *buf)
{
	LOG(3, "bttp %p", bttp);

	memset(buf, '\0', bttp->lbasize);
	return 0;
}

/*
 * lba_to_arena_lba -- (internal) calculate the arena & pre-map LBA
 *
 * This routine takes the external LBA and matches it to the
 * appropriate arena, adjusting the lba for use within that arena.
 *
 * If successful, zero is returned, *arenapp is a pointer to the appropriate
 * arena struct in the run-time state, and *premap_lbap is the LBA adjusted
 * to an arena-internal LBA (also known as the pre-map LBA).  Otherwise
 * -1/errno.
 */
static int
lba_to_arena_lba(struct btt *bttp, uint64_t lba,
		struct arena **arenapp, uint32_t *premap_lbap)
{
	LOG(3, "bttp %p lba %" PRIu64, bttp, lba);

	ASSERT(bttp->laidout);

	unsigned arena;
	for (arena = 0; arena < bttp->narena; arena++)
		if (lba < bttp->arenas[arena].external_nlba)
			break;
		else
			lba -= bttp->arenas[arena].external_nlba;

	ASSERT(arena < bttp->narena);

	*arenapp = &bttp->arenas[arena];
	ASSERT(lba <= UINT32_MAX);
	*premap_lbap = (uint32_t)lba;

	LOG(3, "arenap %p pre-map LBA %u", *arenapp, *premap_lbap);
	return 0;
}

/*
 * btt_init -- prepare a btt namespace for use, returning an opaque handle
 *
 * Returns handle on success, otherwise NULL/errno.
 *
 * When submitted a pristine namespace it will be formatted implicitly when
 * touched for the first time.
 *
 * If arenas have different nfree values, we will be using the lowest one
 * found as limiting to the overall "bandwidth".
 */
struct btt *
btt_init(uint64_t rawsize, uint32_t lbasize, uint8_t parent_uuid[],
		unsigned maxlane, void *ns, const struct ns_callback *ns_cbp)
{
	LOG(3, "rawsize %" PRIu64 " lbasize %u", rawsize, lbasize);

	if (rawsize < BTT_MIN_SIZE) {
		ERR("rawsize smaller than BTT_MIN_SIZE %u", BTT_MIN_SIZE);
		errno = EINVAL;
		return NULL;
	}

	struct btt *bttp = Zalloc(sizeof(*bttp));

	if (bttp == NULL) {
		ERR("!Malloc %zu bytes", sizeof(*bttp));
		return NULL;
	}

	util_mutex_init(&bttp->layout_write_mutex);
	memcpy(bttp->parent_uuid, parent_uuid, BTTINFO_UUID_LEN);
	bttp->rawsize = rawsize;
	bttp->lbasize = lbasize;
	bttp->ns = ns;
	bttp->ns_cbp = ns_cbp;

	/*
	 * Load up layout, if it exists.
	 *
	 * Whether read_layout() finds a valid layout or not, it finishes
	 * updating these layout-related fields:
	 *	bttp->nfree
	 *	bttp->nlba
	 *	bttp->narena
	 * since these fields are used even before a valid layout it written.
	 */
	if (read_layout(bttp, 0) < 0) {
		btt_fini(bttp);		/* free up any allocations */
		return NULL;
	}

	bttp->nlane = bttp->nfree;

	/* maxlane, if provided, is an upper bound on nlane */
	if (maxlane && bttp->nlane > maxlane)
		bttp->nlane = maxlane;

	LOG(3, "success, bttp %p nlane %u", bttp, bttp->nlane);
	return bttp;
}

/*
 * btt_nlane -- return the number of "lanes" for this btt namespace
 *
 * The number of lanes is the number of threads allowed in this module
 * concurrently for a given btt.  Each thread executing this code must
 * have a unique "lane" number assigned to it between 0 and btt_nlane() - 1.
 */
unsigned
btt_nlane(struct btt *bttp)
{
	LOG(3, "bttp %p", bttp);

	return bttp->nlane;
}

/*
 * btt_nlba -- return the number of usable blocks in a btt namespace
 *
 * Valid LBAs to pass to btt_read() and btt_write() are 0 through
 * btt_nlba() - 1.
 */
size_t
btt_nlba(struct btt *bttp)
{
	LOG(3, "bttp %p", bttp);

	return bttp->nlba;
}

/*
 * btt_read -- read a block from a btt namespace
 *
 * Returns 0 on success, otherwise -1/errno.
 */
int
btt_read(struct btt *bttp, unsigned lane, uint64_t lba, void *buf)
{
	LOG(3, "bttp %p lane %u lba %" PRIu64, bttp, lane, lba);

	if (invalid_lba(bttp, lba))
		return -1;

	/* if there's no layout written yet, all reads come back as zeros */
	if (!bttp->laidout)
		return zero_block(bttp, buf);

	/* find which arena LBA lives in, and the offset to the map entry */
	struct arena *arenap;
	uint32_t premap_lba;
	uint64_t map_entry_off;
	if (lba_to_arena_lba(bttp, lba, &arenap, &premap_lba) < 0)
		return -1;

	/*
	 * Read the current map entry to get the post-map LBA for the data
	 * block read.
	 */
	uint32_t entry;

	/* convert pre-map LBA into an offset into the map */
	map_entry_off = arenap->mapoff + BTT_MAP_ENTRY_SIZE * premap_lba;

	if ((*bttp->ns_cbp->nsread)(bttp->ns, lane, &entry,
				sizeof(entry), map_entry_off) < 0)
		return -1;

	entry = le32toh(entry);
	/*
	 * Retries come back to the top of this loop (for a rare case where
	 * the map is changed by another thread doing writes to the same LBA).
	 */
	while (1) {
		if (map_entry_is_error(entry)) {
			ERR("EIO due to map entry error flag");
			errno = EIO;
			return -1;
		}

		if (map_entry_is_zero_or_initial(entry))
			return zero_block(bttp, buf);

		/*
		 * Record the post-map LBA in the read tracking table during
		 * the read.  The write will check entries in the read tracking
		 * table before allocating a block for a write, waiting for
		 * outstanding reads on that block to complete.
		 *
		 * Since we already checked for error, zero, and initial
		 * states above, the entry must have both error and zero
		 * bits set at this point (BTT_MAP_ENTRY_NORMAL).  We store
		 * the entry that way, with those bits set, in the rtt and
		 * btt_write() will check for it the same way, with the bits
		 * both set.
		 */
		arenap->rtt[lane] = entry;
		util_synchronize();

		/*
		 * In case this thread was preempted between reading entry and
		 * storing it in the rtt, check to see if the map changed.  If
		 * it changed, the block about to be read is at least free now
		 * (in the flog, but that's okay since the data will still be
		 * undisturbed) and potentially allocated and being used for
		 * another write (data disturbed, so not okay to continue).
		 */
		uint32_t latest_entry;
		if ((*bttp->ns_cbp->nsread)(bttp->ns, lane, &latest_entry,
				sizeof(latest_entry), map_entry_off) < 0) {
			arenap->rtt[lane] = BTT_MAP_ENTRY_ERROR;
			return -1;
		}

		latest_entry = le32toh(latest_entry);

		if (entry == latest_entry)
			break;			/* map stayed the same */
		else
			entry = latest_entry;	/* try again */
	}

	/*
	 * It is safe to read the block now, since the rtt protects the
	 * block from getting re-allocated to something else by a write.
	 */
	uint64_t data_block_off =
		arenap->dataoff + (uint64_t)(entry & BTT_MAP_ENTRY_LBA_MASK) *
		arenap->internal_lbasize;
	int readret = (*bttp->ns_cbp->nsread)(bttp->ns, lane, buf,
					bttp->lbasize, data_block_off);

	/* done with read, so clear out rtt entry */
	arenap->rtt[lane] = BTT_MAP_ENTRY_ERROR;

	return readret;
}

/*
 * map_lock -- (internal) grab the map_lock and read a map entry
 */
static int
map_lock(struct btt *bttp, unsigned lane, struct arena *arenap,
		uint32_t *entryp, uint32_t premap_lba)
{
	LOG(3, "bttp %p lane %u arenap %p premap_lba %u",
			bttp, lane, arenap, premap_lba);

	uint64_t map_entry_off =
			arenap->mapoff + BTT_MAP_ENTRY_SIZE * premap_lba;
	uint32_t map_lock_num = get_map_lock_num(premap_lba, bttp->nfree);

	util_mutex_lock(&arenap->map_locks[map_lock_num]);

	/* read the old map entry */
	if ((*bttp->ns_cbp->nsread)(bttp->ns, lane, entryp,
				sizeof(uint32_t), map_entry_off) < 0) {
		util_mutex_unlock(&arenap->map_locks[map_lock_num]);
		return -1;
	}

	if (arenap->major == 1) {
		/* if map entry is in its initial state return premap_lba */
		if (map_entry_is_initial(*entryp))
			*entryp = htole32(premap_lba | BTT_MAP_ENTRY_NORMAL);
	} else {
		if (map_entry_is_initial(*entryp)) {
			get_lane_aba(arenap, lane, entryp);
		}
		else
			arenap->lane_free[lane] = *entryp;
	}

	LOG(9, "locked map[%d]: %u%s%s", premap_lba,
			*entryp & BTT_MAP_ENTRY_LBA_MASK,
			(map_entry_is_error(*entryp)) ? " ERROR" : "",
			(map_entry_is_zero(*entryp)) ? " ZERO" : "");

	return 0;
}

/*
 * map_abort -- (internal) drop the map_lock without updating the entry
 */
static void
map_abort(struct btt *bttp, unsigned lane, struct arena *arenap,
		uint32_t premap_lba)
{
	LOG(3, "bttp %p lane %u arenap %p premap_lba %u",
			bttp, lane, arenap, premap_lba);

	util_mutex_unlock(&arenap->map_locks[get_map_lock_num(premap_lba,
				bttp->nfree)]);
}

/*
 * map_unlock -- (internal) update the map and drop the map_lock
 */
static int
map_unlock(struct btt *bttp, unsigned lane, struct arena *arenap,
		uint32_t entry, uint32_t premap_lba)
{
	LOG(3, "bttp %p lane %u arenap %p entry %u premap_lba %u",
			bttp, lane, arenap, entry, premap_lba);

	uint64_t map_entry_off =
			arenap->mapoff + BTT_MAP_ENTRY_SIZE * premap_lba;

	/* write the new map entry */
	int err = (*bttp->ns_cbp->nswrite)(bttp->ns, lane, &entry,
				sizeof(uint32_t), map_entry_off);

	util_mutex_unlock(&arenap->map_locks[get_map_lock_num(premap_lba,
				bttp->nfree)]);

	LOG(9, "unlocked map[%d]: %u%s%s", premap_lba,
			entry & BTT_MAP_ENTRY_LBA_MASK,
			(map_entry_is_error(entry)) ? " ERROR" : "",
			(map_entry_is_zero(entry)) ? " ZERO" : "");

	return err;
}

/*
 * btt_write -- write a block to a btt namespace
 *
 * Returns 0 on success, otherwise -1/errno.
 */
int
btt_write(struct btt *bttp, unsigned lane, uint64_t lba, const void *buf)
{
	LOG(3, "bttp %p lane %u lba %" PRIu64, bttp, lane, lba);

	if (invalid_lba(bttp, lba))
		return -1;

	/* first write through here will initialize the metadata layout */
	if (!bttp->laidout) {
		int err = 0;

		util_mutex_lock(&bttp->layout_write_mutex);

		if (!bttp->laidout)
			err = write_layout(bttp, lane, 1);

		util_mutex_unlock(&bttp->layout_write_mutex);

		if (err < 0)
			return err;
	}

	/* find which arena LBA lives in, and the offset to the map entry */
	struct arena *arenap;
	uint32_t premap_lba;
	if (lba_to_arena_lba(bttp, lba, &arenap, &premap_lba) < 0)
		return -1;

	/* if the arena is in an error state, writing is not allowed */
	if (arenap->flags & BTTINFO_FLAG_ERROR_MASK) {
		ERR("EIO due to btt_info error flags 0x%x",
			arenap->flags & BTTINFO_FLAG_ERROR_MASK);
		errno = EIO;
		return -1;
	}

	uint32_t free_entry;
	if (arenap->major == 1) {
		/*
		 * This routine was passed a unique "lane" which is an index
		 * into the flog.  That means the free block held by flog[lane]
		 * is assigned to this thread and to no other threads
		 * (no additional locking required).
		 * So start by performing the write to the
		 * free block.  It is only safe to write to a free block if it
		 * doesn't appear in the read tracking table, so scan that first
		 * and if found, wait for the thread reading from it to finish.
		 */
		free_entry = (arenap->flogs[lane].flog.old_map &
				BTT_MAP_ENTRY_LBA_MASK) | BTT_MAP_ENTRY_NORMAL;

		LOG(3, "free_entry %u (before mask %u)", free_entry,
			arenap->flogs[lane].flog.old_map);
		} else {
		free_entry = arenap->lane_free[lane] | BTT_MAP_ENTRY_NORMAL;
	}

	/* wait for other threads to finish any reads on free block */
	for (unsigned i = 0; i < bttp->nlane; i++)
		while (arenap->rtt[i] == free_entry)
			;

	/*
	 * it is now safe to perform write to the free block
	 * if write fail, we discard this block or put back to free?
	 * when read lba, keep the lba with previous data
	 */
	uint64_t data_block_off = arenap->dataoff +
		(uint64_t)(free_entry & BTT_MAP_ENTRY_LBA_MASK) *
			arenap->internal_lbasize;
	if ((*bttp->ns_cbp->nswrite)(bttp->ns, lane, buf,
				bttp->lbasize, data_block_off) < 0)
		return -1;

	/*
	 * Make the new block active atomically by updating the on-media flog
	 * and then updating the map.
	 */
	uint32_t old_entry;
	if (map_lock(bttp, lane, arenap, &old_entry, premap_lba) < 0)
		return -1;

	if (arenap->major == 1) {
		old_entry = le32toh(old_entry);
		/* update the flog */
		if (flog_update(bttp, lane, arenap, premap_lba,
						old_entry, free_entry) < 0) {
			map_abort(bttp, lane, arenap, premap_lba);
			return -1;
		}
	}

	if (map_unlock(bttp, lane, arenap, htole32(free_entry),
					premap_lba) < 0) {
		/*
		 * A critical write error occurred, set the arena's
		 * info block error bit.
		 */
		set_arena_error(bttp, arenap, lane);
		errno = EIO;
		return -1;
	}

	return 0;
}

/*
 * map_entry_setf -- (internal) set a given flag on a map entry
 *
 * Returns 0 on success, otherwise -1/errno.
 */
static int
map_entry_setf(struct btt *bttp, unsigned lane, uint64_t lba, uint32_t setf)
{
	LOG(3, "bttp %p lane %u lba %" PRIu64 " setf 0x%x",
	    bttp, lane, lba, setf);

	if (invalid_lba(bttp, lba))
		return -1;

	if (!bttp->laidout) {
		/*
		 * No layout is written yet.  If the flag being set
		 * is the zero flag, it is superfluous since all blocks
		 * read as zero at this point.
		 */
		if (setf == BTT_MAP_ENTRY_ZERO)
			return 0;

		/*
		 * Treat this like the first write and write out
		 * the metadata layout at this point.
		 */
		int err = 0;
		util_mutex_lock(&bttp->layout_write_mutex);

		if (!bttp->laidout)
			err = write_layout(bttp, lane, 1);

		util_mutex_unlock(&bttp->layout_write_mutex);

		if (err < 0)
			return err;
	}

	/* find which arena LBA lives in, and the offset to the map entry */
	struct arena *arenap;
	uint32_t premap_lba;
	if (lba_to_arena_lba(bttp, lba, &arenap, &premap_lba) < 0)
		return -1;

	/* if the arena is in an error state, writing is not allowed */
	if (arenap->flags & BTTINFO_FLAG_ERROR_MASK) {
		ERR("EIO due to btt_info error flags 0x%x",
			arenap->flags & BTTINFO_FLAG_ERROR_MASK);
		errno = EIO;
		return -1;
	}

	/*
	 * Set the flags in the map entry.  To do this, read the
	 * current map entry, set the flags, and write out the update.
	 */
	uint32_t old_entry;
	uint32_t new_entry;

	if (map_lock(bttp, lane, arenap, &old_entry, premap_lba) < 0)
		return -1;

	old_entry = le32toh(old_entry);

	if (setf == BTT_MAP_ENTRY_ZERO &&
			map_entry_is_zero_or_initial(old_entry)) {
		map_abort(bttp, lane, arenap, premap_lba);
		return 0;	/* block already zero, nothing to do */
	}

	/* create the new map entry */
	new_entry = (old_entry & BTT_MAP_ENTRY_LBA_MASK) | setf;

	if (map_unlock(bttp, lane, arenap, htole32(new_entry), premap_lba) < 0)
		return -1;

	return 0;
}

/*
 * btt_set_zero -- mark a block as zeroed in a btt namespace
 *
 * Returns 0 on success, otherwise -1/errno.
 */
int
btt_set_zero(struct btt *bttp, unsigned lane, uint64_t lba)
{
	LOG(3, "bttp %p lane %u lba %" PRIu64, bttp, lane, lba);

	return map_entry_setf(bttp, lane, lba, BTT_MAP_ENTRY_ZERO);
}

/*
 * btt_set_error -- mark a block as in an error state in a btt namespace
 *
 * Returns 0 on success, otherwise -1/errno.
 */
int
btt_set_error(struct btt *bttp, unsigned lane, uint64_t lba)
{
	LOG(3, "bttp %p lane %u lba %" PRIu64, bttp, lane, lba);

	return map_entry_setf(bttp, lane, lba, BTT_MAP_ENTRY_ERROR);
}

/*
 * check_arena -- (internal) perform a consistency check on an arena
 */
static int
check_arena(struct btt *bttp, struct arena *arenap)
{
	LOG(3, "bttp %p arenap %p", bttp, arenap);

	int consistent = 1;

	uint64_t map_entry_off = arenap->mapoff;
	uint32_t bitmapsize = howmany(arenap->internal_nlba, 8);
	uint8_t *bitmap = Zalloc(bitmapsize);
	if (bitmap == NULL) {
		ERR("!Malloc for bitmap");
		return -1;
	}

	/*
	 * Go through every post-map LBA mentioned in the map and make sure
	 * there are no duplicates.  bitmap is used to track which LBAs have
	 * been seen so far.
	 */
	uint32_t *mapp = NULL;
	ssize_t mlen;
	int next_index = 0;
	size_t remaining = 0;
	for (uint32_t i = 0; i < arenap->external_nlba; i++) {
		uint32_t entry;

		if (remaining == 0) {
			/* request a mapping of remaining map area */
			size_t req_len =
				(arenap->external_nlba - i) * sizeof(uint32_t);
			mlen = (*bttp->ns_cbp->nsmap)(bttp->ns, 0,
				(void **)&mapp, req_len, map_entry_off);

			if (mlen < 0)
				return -1;

			remaining = (size_t)mlen;
			next_index = 0;
		}
		entry = le32toh(mapp[next_index]);

		/* for debug, dump non-zero map entries at log level 11 */
		if (map_entry_is_zero_or_initial(entry) == 0)
			LOG(11, "map[%d]: %u%s", i,
				entry &	BTT_MAP_ENTRY_LBA_MASK,
				(map_entry_is_error(entry)) ? " ERROR" : "");

		/* this is an uninitialized map entry, set the default value */
		if (map_entry_is_initial(entry)) {
			if (arenap->major == 1) entry = i;
			else goto skip_check;
		}
		else
			entry &= BTT_MAP_ENTRY_LBA_MASK;

		/* check if entry is valid */
		if (entry >= arenap->internal_nlba) {
			ERR("map[%d] entry out of bounds: %u", i, entry);
			errno = EINVAL;
			return -1;
		}

		if (util_isset(bitmap, entry)) {
			ERR("map[%d] duplicate entry: %u", i, entry);
			consistent = 0;
		} else
			util_setbit(bitmap, entry);

skip_check:
		map_entry_off += sizeof(uint32_t);
		next_index++;
		ASSERT(remaining >= sizeof(uint32_t));
		remaining -= sizeof(uint32_t);
	}

	if (arenap->major == 1) {
		/*
		 * Go through the free blocks in the flog, adding them to bitmap
		 * and checking for duplications.  It is sufficient to read the
		 * run-time flog here, avoiding more calls to nsread.
		 */
		for (uint32_t i = 0; i < bttp->nfree; i++) {
			uint32_t entry = arenap->flogs[i].flog.old_map;
			entry &= BTT_MAP_ENTRY_LBA_MASK;

			if (util_isset(bitmap, entry)) {
				ERR("flog[%u] duplicate entry: %u", i, entry);
				consistent = 0;
			} else
				util_setbit(bitmap, entry);
		}

		/*
		 * Make sure every possible post-map LBA was accounted for
		 * in the two loops above.
		 */
		for (uint32_t i = 0; i < arenap->internal_nlba; i++)
		if (util_isclr(bitmap, i)) {
			ERR("unreferenced lba: %d", i);
			consistent = 0;
		}
	}

	Free(bitmap);

	return consistent;
}

/*
 * btt_check -- perform a consistency check on a btt namespace
 *
 * This routine contains a fairly high-impact set of consistency checks.
 * It may use a good amount of dynamic memory and CPU time performing
 * the checks.  Any lightweight, quick consistency checks are included
 * in read_layout() so they happen every time the BTT area is opened
 * for use.
 *
 * Returns true if consistent, zero if inconsistent, -1/error if checking
 * cannot happen due to other errors.
 *
 * No lane number required here because only one thread is allowed -- all
 * other threads must be locked out of all btt routines for this btt
 * namespace while this is running.
 */
int
btt_check(struct btt *bttp)
{
	LOG(3, "bttp %p", bttp);

	int consistent = 1;

	if (!bttp->laidout) {
		/* consistent by definition */
		LOG(3, "no layout yet");
		return consistent;
	}

	/* XXX report issues found during read_layout (from flags) */

	/* for each arena... */
	struct arena *arenap = bttp->arenas;
	for (unsigned i = 0; i < bttp->narena; i++, arenap++) {
		/*
		 * Perform the consistency checks for the arena.
		 */
		int retval = check_arena(bttp, arenap);
		if (retval < 0)
			return retval;
		else if (retval == 0)
			consistent = 0;
	}

	/* XXX stub */
	return consistent;
}

/*
 * btt_fini -- delete opaque btt info, done using btt namespace
 */
void
btt_fini(struct btt *bttp)
{
	LOG(3, "bttp %p", bttp);
	uint32_t *free_data;

	if (bttp->arenas) {
		for (unsigned i = 0; i < bttp->narena; i++) {
			if (bttp->arenas[i].major == 1) {
				if (bttp->arenas[i].flogs)
					Free(bttp->arenas[i].flogs);
			} else {
				free_data =
					bttp->arenas[i].sd_freelist.free_array;
				if (free_data)
					Free(free_data);
				free_data = bttp->arenas[i].lane_free;
				if (free_data)
					Free(free_data);
			}
			if (bttp->arenas[i].rtt)
				Free((void *)bttp->arenas[i].rtt);
			if (bttp->arenas[i].map_locks)
				Free((void *)bttp->arenas[i].map_locks);
		}
		Free(bttp->arenas);
	}
	Free(bttp);
}
