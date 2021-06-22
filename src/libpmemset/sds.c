// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/*
 * sds.c -- implementation of common SDS API
 */

#include <errno.h>
#include <libpmem2.h>

#include "libpmemset.h"
#include "libpmem2.h"

#include "alloc.h"
#include "config.h"
#include "file.h"
#include "os_thread.h"
#include "pmemset_utils.h"
#include "ravl.h"
#include "sds.h"
#include "sys_util.h"

struct pmemset_sds_record {
	struct pmemset_sds *sds;
	struct pmemset_source *src;
	struct pmem2_map *map;
	size_t refcount;
};

static struct pmemset_sds_state {
	struct ravl *rtree;
	os_rwlock_t sds_lock;
} State;

/*
 * ravl_sds_compare -- compare sds records by its map pointer addresses, order
 *                     doesn't matter in this case
 */
static int
ravl_sds_compare(const void *lhs, const void *rhs)
{
	struct pmemset_sds_record *rec_left = (struct pmemset_sds_record *)lhs;
	struct pmemset_sds_record *rec_right = (struct pmemset_sds_record *)rhs;

	if (rec_left->map > rec_right->map)
		return 1;
	if (rec_left->map < rec_right->map)
		return -1;
	return 0;
}

/*
 * pmemset_sds_init -- initialize sds
 */
void
pmemset_sds_init()
{
	util_rwlock_init(&State.sds_lock);

	util_rwlock_wrlock(&State.sds_lock);
	State.rtree = ravl_new_sized(ravl_sds_compare,
			sizeof(struct pmemset_sds_record));
	util_rwlock_unlock(&State.sds_lock);

	if (!State.rtree)
		abort();
}

/*
 * pmemset_sds_fini -- finalize sds
 */
void
pmemset_sds_fini()
{
	ravl_delete(State.rtree);
	util_rwlock_destroy(&State.sds_lock);
}

/*
 * pmemset_sds_duplicate -- copy sds structure, allocate destination if needed
 */
int
pmemset_sds_duplicate(struct pmemset_sds **sds_dst, struct pmemset_sds *sds_src)
{
	int ret;

	/* allocate dst if needed */
	if (*sds_dst == NULL) {
		*sds_dst = pmemset_malloc(sizeof(**sds_dst), &ret);
		if (ret)
			return ret;
	}

	/* copy sds */
	strncpy((*sds_dst)->id, sds_src->id, PMEMSET_SDS_DEVICE_ID_LEN);
	(*sds_dst)->usc = sds_src->usc;
	(*sds_dst)->refcount = sds_src->refcount;

	return 0;
}

/*
 * pmemset_sds_find_record -- search for SDS record in the ravl tree
 */
struct pmemset_sds_record *
pmemset_sds_find_record(struct pmem2_map *map)
{
	struct pmemset_sds_record sds_record;
	sds_record.map = map;

	struct ravl_node *node;

	util_rwlock_rdlock(&State.sds_lock);
	node = ravl_find(State.rtree, &sds_record, RAVL_PREDICATE_EQUAL);
	util_rwlock_unlock(&State.sds_lock);

	if (!node)
		return NULL;

	return ravl_data(node);
}

/*
 * pmemset_sds_record_get_sds -- get SDS from sds record
 */
struct pmemset_sds *
pmemset_sds_record_get_sds(struct pmemset_sds_record *sds_record)
{
	return sds_record->sds;
}

/*
 * pmemset_sds_record_get_source -- get source from sds record
 */
struct pmemset_source *
pmemset_sds_record_get_source(struct pmemset_sds_record *sds_record)
{
	return sds_record->src;
}

/*
 * pmemset_sds_register_record -- register sds record in the sds tree
 */
int
pmemset_sds_register_record(struct pmemset_sds *sds, struct pmemset_source *src,
		struct pmem2_map *p2map)
{
	struct pmemset_sds_record sds_record;
	sds_record.sds = sds;
	sds_record.src = src;
	sds_record.map = p2map;

	util_rwlock_wrlock(&State.sds_lock);
	sds->refcount += 1;
	int ret = ravl_emplace_copy(State.rtree, &sds_record);
	if (ret)
		goto err_lower_ref;

	util_rwlock_unlock(&State.sds_lock);
	return 0;

err_lower_ref:
	sds->refcount -= 1;
	util_rwlock_unlock(&State.sds_lock);
	return ret;
}

/*
 * pmemset_sds_unregister_record -- unregister sds record from the sds tree
 */
int
pmemset_sds_unregister_record(struct pmemset_sds_record *sds_record)
{
	int ret = 0;
	struct pmemset_sds *sds = sds_record->sds;

	util_rwlock_wrlock(&State.sds_lock);
	struct ravl_node *node = ravl_find(State.rtree, sds_record,
			RAVL_PREDICATE_EQUAL);
	if (!node) {
		ret = -ENOENT;
		goto err_unlock;
	}

	ravl_remove(State.rtree, node);

	sds->refcount -= 1;
	ASSERT(sds->refcount >= 0);

err_unlock:
	util_rwlock_unlock(&State.sds_lock);
	return ret;
}

/*
 * pmemset_sds_new -- allocates and initialize SDS structure
 */
int
pmemset_sds_new(struct pmemset_sds **sds_ptr, struct pmemset_source *src)
{
	LOG(3, "sds_ptr %p src %p", sds_ptr, src);

	*sds_ptr = NULL;
	struct pmemset_file *part_file = pmemset_source_get_set_file(src);
	struct pmem2_source *pmem2_src =
			pmemset_file_get_pmem2_source(part_file);

	int ret = 0;
	struct pmemset_sds *sds = pmemset_malloc(sizeof(*sds), &ret);
	if (ret)
		return ret;

	memset(sds->id, 0, PMEMSET_SDS_DEVICE_ID_LEN);

	/* read device unsafe shutdown count */
	ret = pmem2_source_device_usc(pmem2_src, &sds->usc);
	if (ret) {
		if (ret == PMEM2_E_NOSUPP)
			ret = PMEMSET_E_SDS_ENOSUPP;
		goto err_free_sds;
	}

	/* read device ID length */
	size_t len = 0;
	ret = pmem2_source_device_id(pmem2_src, NULL, &len);
	if (ret) {
		if (ret == PMEM2_E_NOSUPP)
			ret =  PMEMSET_E_SDS_ENOSUPP;
		goto err_free_sds;
	}

	ASSERT(len <= PMEMSET_SDS_DEVICE_ID_LEN);

	/* read device ID */
	ret = pmem2_source_device_id(pmem2_src, (char *)&sds->id, &len);
	if (ret) {
		if (ret == PMEM2_E_NOSUPP)
			ret = PMEMSET_E_SDS_ENOSUPP;
		goto err_free_sds;
	}

	sds->refcount = 0;

	*sds_ptr = sds;

	return 0;

err_free_sds:
	Free(sds);
	return ret;
}

/*
 * pmemset_sds_delete -- deallocate sds structure
 */
int
pmemset_sds_delete(struct pmemset_sds **sds_ptr)
{
	Free(*sds_ptr);
	*sds_ptr = NULL;
	return 0;
}

/*
 * pmemset_sds_is_initialiized -- checks if the SDS was initialized
 */
static bool
pmemset_sds_is_initialiized(const struct pmemset_sds *sds_old,
	const struct pmemset_sds *sds_new)
{
	return strncmp(sds_old->id, sds_new->id,
			PMEMSET_SDS_DEVICE_ID_LEN) == 0;
}

/*
 * pmemset_sds_is_consistent -- checks if the SDS indicates possible data
 *                              corruption
 */
static bool
pmemset_sds_is_consistent(const struct pmemset_sds *sds_old,
		const struct pmemset_sds *sds_new)
{
	return sds_old->usc == sds_new->usc;
}

/*
 * pmemset_sds_state_check_and_possible_refresh -- checks part state and refresh
 *                                                 usc if it's just outdated
 */
int
pmemset_sds_state_check_and_possible_refresh(struct pmemset_source *src,
		enum pmemset_part_state *state_ptr)
{
	struct pmemset_sds *sds = pmemset_source_get_extras(src).sds;

	struct pmemset_sds *sds_new;
	int ret = pmemset_sds_new(&sds_new, src);
	if (ret)
		return ret;

	enum pmemset_part_state state = (sds->refcount > 0) ?
		PMEMSET_PART_STATE_OK_BUT_INTERRUPTED : PMEMSET_PART_STATE_OK;

	bool initialized = pmemset_sds_is_initialiized(sds, sds_new);
	if (initialized) {
		/*
		 * Pool is corrupted only if it wasn't closed cleanly and
		 * the SDS is inconsistent.
		 */
		if (!pmemset_sds_is_consistent(sds, sds_new) &&
				sds->refcount != 0)
			state = PMEMSET_PART_STATE_CORRUPTED;
		else if (!pmemset_sds_is_consistent(sds, sds_new))
			sds->usc = sds_new->usc;

		/*
		 * If SDS indicates inconsistency, but the pool was
		 * not in use, we just need to reinitialize the SDS usc value.
		 */
	} else if (sds->refcount == 0) {
		/* reinitialize SDS on new device */
		strncpy(sds->id, sds_new->id, PMEMSET_SDS_DEVICE_ID_LEN);
		sds->usc = sds_new->usc;
	} else {
		state = PMEMSET_PART_STATE_INDETERMINATE;
	}

	ret = pmemset_sds_delete(&sds_new);
	ASSERTeq(ret, 0);

	*state_ptr = state;

	return 0;
}

/*
 * pmemset_sds_fire_sds_update_event -- fire sds update event
 */
int
pmemset_sds_fire_sds_update_event(struct pmemset *set, struct pmemset_sds *sds,
		struct pmemset_config *cfg, struct pmemset_source *src)
{
	struct pmemset_event_context ctx;
	ctx.type = PMEMSET_EVENT_SDS_UPDATE;
	ctx.data.sds_update.sds = sds;
	ctx.data.sds_update.src = src;

	pmemset_config_event_callback(cfg, set, &ctx);

	return 0;
}
