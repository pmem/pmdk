// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2019, Intel Corporation */

/*
 * check_btt_map_flog.c -- check BTT Map and Flog
 */

#include <stdint.h>
#include <sys/param.h>
#include <endian.h>

#include "out.h"
#include "btt.h"
#include "libpmempool.h"
#include "pmempool.h"
#include "pool.h"
#include "check_util.h"

enum questions {
	Q_REPAIR_MAP,
	Q_REPAIR_FLOG,
};

/*
 * flog_read -- (internal) read and convert flog from file
 */
static int
flog_read(PMEMpoolcheck *ppc, struct arena *arenap)
{
	uint64_t flogoff = arenap->offset + arenap->btt_info.flogoff;
	arenap->flogsize = btt_flog_size(arenap->btt_info.nfree);

	arenap->flog = malloc(arenap->flogsize);
	if (!arenap->flog) {
		ERR("!malloc");
		goto error_malloc;
	}

	if (pool_read(ppc->pool, arenap->flog, arenap->flogsize, flogoff))
		goto error_read;

	uint8_t *ptr = arenap->flog;
	uint32_t i;
	for (i = 0; i < arenap->btt_info.nfree; i++) {
		struct btt_flog *flog = (struct btt_flog *)ptr;
		btt_flog_convert2h(&flog[0]);
		btt_flog_convert2h(&flog[1]);

		ptr += BTT_FLOG_PAIR_ALIGN;
	}

	return 0;

error_read:
	free(arenap->flog);
	arenap->flog = NULL;

error_malloc:
	return -1;
}

/*
 * map_read -- (internal) read and convert map from file
 */
static int
map_read(PMEMpoolcheck *ppc, struct arena *arenap)
{
	uint64_t mapoff = arenap->offset + arenap->btt_info.mapoff;
	arenap->mapsize = btt_map_size(arenap->btt_info.external_nlba);

	ASSERT(arenap->mapsize != 0);
	arenap->map = malloc(arenap->mapsize);
	if (!arenap->map) {
		ERR("!malloc");
		goto error_malloc;
	}

	if (pool_read(ppc->pool, arenap->map, arenap->mapsize, mapoff)) {
		goto error_read;
	}

	uint32_t i;
	for (i = 0; i < arenap->btt_info.external_nlba; i++)
		arenap->map[i] = le32toh(arenap->map[i]);

	return 0;

error_read:
	free(arenap->map);
	arenap->map = NULL;
error_malloc:
	return -1;
}

/*
 * list_item -- item for simple list
 */
struct list_item {
	PMDK_LIST_ENTRY(list_item) next;
	uint32_t val;
};

/*
 * list -- simple list for storing numbers
 */
struct list {
	PMDK_LIST_HEAD(listhead, list_item) head;
	uint32_t count;
};

/*
 * list_alloc -- (internal) allocate an empty list
 */
static struct list *
list_alloc(void)
{
	struct list *list = malloc(sizeof(struct list));
	if (!list) {
		ERR("!malloc");
		return NULL;
	}
	PMDK_LIST_INIT(&list->head);
	list->count = 0;
	return list;
}

/*
 * list_push -- (internal) insert new element to the list
 */
static struct list_item *
list_push(struct list *list, uint32_t val)
{
	struct list_item *item = malloc(sizeof(*item));
	if (!item) {
		ERR("!malloc");
		return NULL;
	}
	item->val = val;
	list->count++;
	PMDK_LIST_INSERT_HEAD(&list->head, item, next);
	return item;
}

/*
 * list_pop -- (internal) pop element from list head
 */
static int
list_pop(struct list *list, uint32_t *valp)
{
	if (!PMDK_LIST_EMPTY(&list->head)) {
		struct list_item *i = PMDK_LIST_FIRST(&list->head);
		PMDK_LIST_REMOVE(i, next);
		if (valp)
			*valp = i->val;
		free(i);

		list->count--;

		return 1;
	}
	return 0;
}

/*
 * list_free -- (internal) free the list
 */
static void
list_free(struct list *list)
{
	while (list_pop(list, NULL))
		;
	free(list);
}

/*
 * cleanup -- (internal) prepare resources for map and flog check
 */
static int
cleanup(PMEMpoolcheck *ppc, location *loc)
{
	LOG(3, NULL);

	if (loc->list_unmap)
		list_free(loc->list_unmap);
	if (loc->list_flog_inval)
		list_free(loc->list_flog_inval);
	if (loc->list_inval)
		list_free(loc->list_inval);
	if (loc->fbitmap)
		free(loc->fbitmap);
	if (loc->bitmap)
		free(loc->bitmap);
	if (loc->dup_bitmap)
		free(loc->dup_bitmap);

	return 0;
}

/*
 * init -- (internal) initialize map and flog check
 */
static int
init(PMEMpoolcheck *ppc, location *loc)
{
	LOG(3, NULL);

	struct arena *arenap = loc->arenap;

	/* read flog and map entries */
	if (flog_read(ppc, arenap)) {
		CHECK_ERR(ppc, "arena %u: cannot read BTT Flog", arenap->id);
		goto error;
	}

	if (map_read(ppc, arenap)) {
		CHECK_ERR(ppc, "arena %u: cannot read BTT Map", arenap->id);
		goto error;
	}

	/* create bitmaps for checking duplicated blocks */
	uint32_t bitmapsize = howmany(arenap->btt_info.internal_nlba, 8);
	loc->bitmap = calloc(bitmapsize, 1);
	if (!loc->bitmap) {
		ERR("!calloc");
		CHECK_ERR(ppc, "arena %u: cannot allocate memory for blocks "
			"bitmap", arenap->id);
		goto error;
	}

	loc->dup_bitmap = calloc(bitmapsize, 1);
	if (!loc->dup_bitmap) {
		ERR("!calloc");
		CHECK_ERR(ppc, "arena %u: cannot allocate memory for "
			"duplicated blocks bitmap", arenap->id);
		goto error;
	}

	loc->fbitmap = calloc(bitmapsize, 1);
	if (!loc->fbitmap) {
		ERR("!calloc");
		CHECK_ERR(ppc, "arena %u: cannot allocate memory for BTT Flog "
			"bitmap", arenap->id);
		goto error;
	}

	/* list of invalid map entries */
	loc->list_inval = list_alloc();
	if (!loc->list_inval) {
		CHECK_ERR(ppc,
			"arena %u: cannot allocate memory for invalid BTT map "
			"entries list", arenap->id);
		goto error;
	}

	/* list of invalid flog entries */
	loc->list_flog_inval = list_alloc();
	if (!loc->list_flog_inval) {
		CHECK_ERR(ppc,
			"arena %u: cannot allocate memory for invalid BTT Flog "
			"entries list", arenap->id);
		goto error;
	}

	/* list of unmapped blocks */
	loc->list_unmap = list_alloc();
	if (!loc->list_unmap) {
		CHECK_ERR(ppc,
			"arena %u: cannot allocate memory for unmaped blocks "
			"list", arenap->id);
		goto error;
	}

	return 0;

error:
	ppc->result = CHECK_RESULT_ERROR;
	cleanup(ppc, loc);
	return -1;
}

/*
 * map_get_postmap_lba -- extract postmap LBA from map entry
 */
static inline uint32_t
map_get_postmap_lba(struct arena *arenap, uint32_t i)
{
	uint32_t entry = arenap->map[i];

	/* if map record is in initial state (flags == 0b00) */
	if (map_entry_is_initial(entry))
		return i;

	/* read postmap LBA otherwise */
	return entry & BTT_MAP_ENTRY_LBA_MASK;
}

/*
 * map_entry_check -- (internal) check single map entry
 */
static int
map_entry_check(PMEMpoolcheck *ppc, location *loc, uint32_t i)
{
	struct arena *arenap = loc->arenap;
	uint32_t lba = map_get_postmap_lba(arenap, i);

	/* add duplicated and invalid entries to list */
	if (lba < arenap->btt_info.internal_nlba) {
		if (util_isset(loc->bitmap, lba)) {
			CHECK_INFO(ppc, "arena %u: BTT Map entry %u duplicated "
				"at %u", arenap->id, lba, i);
			util_setbit(loc->dup_bitmap, lba);
			if (!list_push(loc->list_inval, i))
				return -1;
		} else
			util_setbit(loc->bitmap, lba);
	} else {
		CHECK_INFO(ppc, "arena %u: invalid BTT Map entry at %u",
			arenap->id, i);
		if (!list_push(loc->list_inval, i))
			return -1;
	}

	return 0;
}

/*
 * flog_entry_check -- (internal) check single flog entry
 */
static int
flog_entry_check(PMEMpoolcheck *ppc, location *loc, uint32_t i,
	uint8_t **ptr)
{
	struct arena *arenap = loc->arenap;

	/* flog entry consists of two btt_flog structures */
	struct btt_flog *flog = (struct btt_flog *)*ptr;

	int next;
	struct btt_flog *flog_cur = btt_flog_get_valid(flog, &next);

	/* insert invalid and duplicated indexes to list */
	if (!flog_cur) {
		CHECK_INFO(ppc, "arena %u: invalid BTT Flog entry at %u",
			arenap->id, i);
		if (!list_push(loc->list_flog_inval, i))
			return -1;

		goto next;
	}

	uint32_t entry = flog_cur->old_map & BTT_MAP_ENTRY_LBA_MASK;
	uint32_t new_entry = flog_cur->new_map & BTT_MAP_ENTRY_LBA_MASK;

	/*
	 * Check if lba is in extranal_nlba range, and check if both old_map and
	 * new_map are in internal_nlba range.
	 */
	if (flog_cur->lba >= arenap->btt_info.external_nlba ||
			entry >= arenap->btt_info.internal_nlba ||
			new_entry >= arenap->btt_info.internal_nlba) {
		CHECK_INFO(ppc, "arena %u: invalid BTT Flog entry at %u",
			arenap->id, i);
		if (!list_push(loc->list_flog_inval, i))
			return -1;

		goto next;
	}

	if (util_isset(loc->fbitmap, entry)) {
		/*
		 * here we have two flog entries which holds the same free block
		 */
		CHECK_INFO(ppc, "arena %u: duplicated BTT Flog entry at %u\n",
			arenap->id, i);
		if (!list_push(loc->list_flog_inval, i))
			return -1;
	} else if (util_isset(loc->bitmap, entry)) {
		/* here we have probably an unfinished write */
		if (util_isset(loc->bitmap, new_entry)) {
			/* Both old_map and new_map are already used in map. */
			CHECK_INFO(ppc, "arena %u: duplicated BTT Flog entry "
				"at %u", arenap->id, i);
			util_setbit(loc->dup_bitmap, new_entry);
			if (!list_push(loc->list_flog_inval, i))
				return -1;
		} else {
			/*
			 * Unfinished write. Next time pool is opened, the map
			 * will be updated to new_map.
			 */
			util_setbit(loc->bitmap, new_entry);
			util_setbit(loc->fbitmap, entry);
		}
	} else {
		int flog_valid = 1;
		/*
		 * Either flog entry is in its initial state:
		 * - current_btt_flog entry is first one in pair and
		 * - current_btt_flog.old_map == current_btt_flog.new_map and
		 * - current_btt_flog.seq == 0b01 and
		 * - second flog entry in pair is zeroed
		 * or
		 * current_btt_flog.old_map != current_btt_flog.new_map
		 */
		if (entry == new_entry)
			flog_valid = (next == 1) && (flog_cur->seq == 1) &&
				util_is_zeroed((const void *)&flog[1],
				sizeof(flog[1]));

		if (flog_valid) {
			/* totally fine case */
			util_setbit(loc->bitmap, entry);
			util_setbit(loc->fbitmap, entry);
		} else {
			CHECK_INFO(ppc, "arena %u: invalid BTT Flog entry at "
				"%u", arenap->id, i);
			if (!list_push(loc->list_flog_inval, i))
				return -1;
		}
	}

next:
	*ptr += BTT_FLOG_PAIR_ALIGN;
	return 0;
}

/*
 * arena_map_flog_check -- (internal) check map and flog
 */
static int
arena_map_flog_check(PMEMpoolcheck *ppc, location *loc)
{
	LOG(3, NULL);

	struct arena *arenap = loc->arenap;

	/* check map entries */
	uint32_t i;
	for (i = 0; i < arenap->btt_info.external_nlba; i++) {
		if (map_entry_check(ppc, loc, i))
			goto error_push;
	}

	/* check flog entries */
	uint8_t *ptr = arenap->flog;
	for (i = 0; i < arenap->btt_info.nfree; i++) {
		if (flog_entry_check(ppc, loc, i, &ptr))
			goto error_push;
	}

	/* check unmapped blocks and insert to list */
	for (i = 0; i < arenap->btt_info.internal_nlba; i++) {
		if (!util_isset(loc->bitmap, i)) {
			CHECK_INFO(ppc, "arena %u: unmapped block %u",
				arenap->id, i);
			if (!list_push(loc->list_unmap, i))
				goto error_push;
		}
	}

	if (loc->list_unmap->count)
		CHECK_INFO(ppc, "arena %u: number of unmapped blocks: %u",
			arenap->id, loc->list_unmap->count);
	if (loc->list_inval->count)
		CHECK_INFO(ppc, "arena %u: number of invalid BTT Map entries: "
			"%u", arenap->id, loc->list_inval->count);
	if (loc->list_flog_inval->count)
		CHECK_INFO(ppc, "arena %u: number of invalid BTT Flog entries: "
			"%u", arenap->id, loc->list_flog_inval->count);

	if (CHECK_IS_NOT(ppc, REPAIR) && loc->list_unmap->count > 0) {
		ppc->result = CHECK_RESULT_NOT_CONSISTENT;
		check_end(ppc->data);
		goto cleanup;
	}

	/*
	 * We are able to repair if and only if number of unmapped blocks is
	 * equal to sum of invalid map and flog entries.
	 */
	if (loc->list_unmap->count != (loc->list_inval->count +
			loc->list_flog_inval->count)) {
		ppc->result = CHECK_RESULT_CANNOT_REPAIR;
		CHECK_ERR(ppc, "arena %u: cannot repair BTT Map and Flog",
			arenap->id);
		goto cleanup;
	}

	if (CHECK_IS_NOT(ppc, ADVANCED) && loc->list_inval->count +
			loc->list_flog_inval->count > 0) {
		ppc->result = CHECK_RESULT_CANNOT_REPAIR;
		CHECK_INFO(ppc, REQUIRE_ADVANCED);
		CHECK_ERR(ppc, "BTT Map and / or BTT Flog contain invalid "
			"entries");
		check_end(ppc->data);
		goto cleanup;
	}

	if (loc->list_inval->count > 0) {
		CHECK_ASK(ppc, Q_REPAIR_MAP, "Do you want to repair invalid "
			"BTT Map entries?");
	}

	if (loc->list_flog_inval->count > 0) {
		CHECK_ASK(ppc, Q_REPAIR_FLOG, "Do you want to repair invalid "
			"BTT Flog entries?");
	}

	return check_questions_sequence_validate(ppc);

error_push:
	CHECK_ERR(ppc, "arena %u: cannot allocate momory for list item",
			arenap->id);
	ppc->result = CHECK_RESULT_ERROR;
cleanup:
	cleanup(ppc, loc);
	return -1;
}

/*
 * arena_map_flog_fix -- (internal) fix map and flog
 */
static int
arena_map_flog_fix(PMEMpoolcheck *ppc, location *loc, uint32_t question,
	void *ctx)
{
	LOG(3, NULL);

	ASSERTeq(ctx, NULL);
	ASSERTne(loc, NULL);

	struct arena *arenap = loc->arenap;
	uint32_t inval;
	uint32_t unmap;
	switch (question) {
	case Q_REPAIR_MAP:
		/*
		 * Cause first of duplicated map entries seems valid till we
		 * find second of them we must find all first map entries
		 * pointing to the postmap LBA's we know are duplicated to mark
		 * them with error flag.
		 */
		for (uint32_t i = 0; i < arenap->btt_info.external_nlba; i++) {
			uint32_t lba = map_get_postmap_lba(arenap, i);
			if (lba >= arenap->btt_info.internal_nlba)
				continue;

			if (!util_isset(loc->dup_bitmap, lba))
				continue;

			arenap->map[i] = BTT_MAP_ENTRY_ERROR | lba;
			util_clrbit(loc->dup_bitmap, lba);
			CHECK_INFO(ppc,
				"arena %u: storing 0x%x at %u BTT Map entry",
				arenap->id, arenap->map[i], i);
		}

		/*
		 * repair invalid or duplicated map entries by using unmapped
		 * blocks
		 */
		while (list_pop(loc->list_inval, &inval)) {
			if (!list_pop(loc->list_unmap, &unmap)) {
				ppc->result = CHECK_RESULT_ERROR;
				return -1;
			}
			arenap->map[inval] = unmap | BTT_MAP_ENTRY_ERROR;
			CHECK_INFO(ppc, "arena %u: storing 0x%x at %u BTT Map "
				"entry", arenap->id, arenap->map[inval], inval);
		}
		break;
	case Q_REPAIR_FLOG:
		/* repair invalid flog entries using unmapped blocks */
		while (list_pop(loc->list_flog_inval, &inval)) {
			if (!list_pop(loc->list_unmap, &unmap)) {
				ppc->result = CHECK_RESULT_ERROR;
				return -1;
			}

			struct btt_flog *flog = (struct btt_flog *)
				(arenap->flog + inval * BTT_FLOG_PAIR_ALIGN);
			memset(&flog[1], 0, sizeof(flog[1]));
			uint32_t entry = unmap | BTT_MAP_ENTRY_ERROR;
			flog[0].lba = inval;
			flog[0].new_map = entry;
			flog[0].old_map = entry;
			flog[0].seq = 1;

			CHECK_INFO(ppc, "arena %u: repairing BTT Flog at %u "
				"with free block entry 0x%x", loc->arenap->id,
				inval, entry);
		}
		break;
	default:
		ERR("not implemented question id: %u", question);
	}

	return 0;
}

struct step {
	int (*check)(PMEMpoolcheck *, location *);
	int (*fix)(PMEMpoolcheck *, location *, uint32_t, void *);
};

static const struct step steps[] = {
	{
		.check	= init,
	},
	{
		.check	= arena_map_flog_check,
	},
	{
		.fix	= arena_map_flog_fix,
	},
	{
		.check	= cleanup,
	},
	{
		.check	= NULL,
		.fix	= NULL,
	},
};

/*
 * step_exe -- (internal) perform single step according to its parameters
 */
static inline int
step_exe(PMEMpoolcheck *ppc, location *loc)
{
	ASSERT(loc->step < ARRAY_SIZE(steps));

	const struct step *step = &steps[loc->step++];

	if (!step->fix)
		return step->check(ppc, loc);

	if (!check_answer_loop(ppc, loc, NULL, 1, step->fix))
		return 0;

	cleanup(ppc, loc);
	return -1;
}

/*
 * check_btt_map_flog -- perform check and fixing of map and flog
 */
void
check_btt_map_flog(PMEMpoolcheck *ppc)
{
	LOG(3, NULL);

	location *loc = check_get_step_data(ppc->data);

	if (ppc->pool->blk_no_layout)
		return;

	/* initialize check */
	if (!loc->arenap && loc->narena == 0 &&
			ppc->result != CHECK_RESULT_PROCESS_ANSWERS) {
		CHECK_INFO(ppc, "checking BTT Map and Flog");
		loc->arenap = PMDK_TAILQ_FIRST(&ppc->pool->arenas);
		loc->narena = 0;
	}

	while (loc->arenap != NULL) {
		/* add info about checking next arena */
		if (ppc->result != CHECK_RESULT_PROCESS_ANSWERS &&
				loc->step == 0) {
			CHECK_INFO(ppc, "arena %u: checking BTT Map and Flog",
				loc->narena);
		}

		/* do all checks */
		while (CHECK_NOT_COMPLETE(loc, steps)) {
			if (step_exe(ppc, loc))
				return;
		}

		/* jump to next arena */
		loc->arenap = PMDK_TAILQ_NEXT(loc->arenap, next);
		loc->narena++;
		loc->step = 0;
	}
}
