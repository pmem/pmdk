// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2024, Intel Corporation */

/*
 * shutdown_state.c -- unsafe shudown detection
 */

#include <string.h>
#include <stdbool.h>
#include <endian.h>
#include "shutdown_state.h"
#include "out.h"
#include "util.h"
#include "os_deep.h"
#include "set.h"
#include "libpmem2.h"
#include "badblocks.h"
#include "../libpmem2/pmem2_utils.h"

#define FLUSH_SDS(sds, rep) \
	if ((rep) != NULL) os_part_deep_common(rep, 0, sds, sizeof(*(sds)), 1)

/*
 * shutdown_state_checksum -- (internal) counts SDS checksum and flush it
 */
static void
shutdown_state_checksum(struct shutdown_state *sds, struct pool_replica *rep)
{
	LOG(3, "sds %p", sds);

	util_checksum(sds, sizeof(*sds), &sds->checksum, 1, 0);
	FLUSH_SDS(sds, rep);
}

/*
 * shutdown_state_init -- initializes shutdown_state struct
 */
int
shutdown_state_init(struct shutdown_state *sds, struct pool_replica *rep)
{
	/* check if we didn't change size of shutdown_state accidentally */
	COMPILE_ERROR_ON(sizeof(struct shutdown_state) != 64);
	LOG(3, "sds %p", sds);

	memset(sds, 0, sizeof(*sds));

	shutdown_state_checksum(sds, rep);

	return 0;
}

/*
 * shutdown_state_add_part -- adds file uuid and usc to shutdown_state struct
 *
 * if path does not exist it will fail which does NOT mean shutdown failure
 */
int
shutdown_state_add_part(struct shutdown_state *sds, int fd,
	struct pool_replica *rep)
{
	LOG(3, "sds %p, fd %d", sds, fd);

	size_t len = 0;
	char *uid;
	uint64_t usc;

	struct pmem2_source *src;

	if (pmem2_source_from_fd(&src, fd))
		return 1;

	int ret = pmem2_source_device_usc(src, &usc);

	if (ret != 0) {
		if (ret == -EPERM) {
			/* overwrite error message */
			ERR_WO_ERRNO(
				"Cannot read unsafe shutdown count. For more information please check https://github.com/pmem/pmdk/issues/4207");
		}
		CORE_LOG_ERROR("cannot read unsafe shutdown count for %d",
			fd);
		goto err;
	}

	ret = pmem2_source_device_id(src, NULL, &len);
	if (ret != 0) {
		ERR_WO_ERRNO("cannot read uuid of %d", fd);
		goto err;
	}

	len += 4 - len % 4;
	uid = Zalloc(len);

	if (uid == NULL) {
		ERR_W_ERRNO("Zalloc");
		goto err;
	}

	ret = pmem2_source_device_id(src, uid, &len);
	if (ret != 0) {
		ERR_WO_ERRNO("cannot read uuid of %d", fd);
		Free(uid);
		goto err;
	}

	sds->usc = htole64(le64toh(sds->usc) + usc);

	uint64_t tmp;
	util_checksum(uid, len, &tmp, 1, 0);
	sds->uuid = htole64(le64toh(sds->uuid) + tmp);

	FLUSH_SDS(sds, rep);
	Free(uid);
	pmem2_source_delete(&src);
	shutdown_state_checksum(sds, rep);
	return 0;
err:
	pmem2_source_delete(&src);
	return 1;
}

/*
 * shutdown_state_set_dirty -- sets dirty pool flag
 */
void
shutdown_state_set_dirty(struct shutdown_state *sds, struct pool_replica *rep)
{
	LOG(3, "sds %p", sds);

	sds->dirty = 1;
	rep->part[0].sds_dirty_modified = 1;

	FLUSH_SDS(sds, rep);

	shutdown_state_checksum(sds, rep);
}

/*
 * shutdown_state_clear_dirty -- clears dirty pool flag
 */
void
shutdown_state_clear_dirty(struct shutdown_state *sds, struct pool_replica *rep)
{
	LOG(3, "sds %p", sds);

	struct pool_set_part part = rep->part[0];
	/*
	 * If a dirty flag was set in previous program execution it should be
	 * preserved as it stores information about potential ADR failure.
	 */
	if (part.sds_dirty_modified != 1)
		return;

	sds->dirty = 0;
	part.sds_dirty_modified = 0;

	FLUSH_SDS(sds, rep);

	shutdown_state_checksum(sds, rep);
}

/*
 * shutdown_state_reinit -- (internal) reinitializes shutdown_state struct
 */
static void
shutdown_state_reinit(struct shutdown_state *curr_sds,
	struct shutdown_state *pool_sds, struct pool_replica *rep)
{
	LOG(3, "curr_sds %p, pool_sds %p", curr_sds, pool_sds);
	shutdown_state_init(pool_sds, rep);
	pool_sds->uuid = htole64(curr_sds->uuid);
	pool_sds->usc = htole64(curr_sds->usc);
	pool_sds->dirty = 0;

	FLUSH_SDS(pool_sds, rep);

	shutdown_state_checksum(pool_sds, rep);
}

/*
 * shutdown_state_check -- compares and fixes shutdown state
 */
int
shutdown_state_check(struct shutdown_state *curr_sds,
	struct shutdown_state *pool_sds, struct pool_replica *rep)
{
	LOG(3, "curr_sds %p, pool_sds %p", curr_sds, pool_sds);

	if (util_is_zeroed(pool_sds, sizeof(*pool_sds)) &&
			!util_is_zeroed(curr_sds, sizeof(*curr_sds))) {
		shutdown_state_reinit(curr_sds, pool_sds, rep);
		return 0;
	}

	bool is_uuid_usc_correct =
		le64toh(pool_sds->usc) == le64toh(curr_sds->usc) &&
		le64toh(pool_sds->uuid) == le64toh(curr_sds->uuid);

	bool is_checksum_correct = util_checksum(pool_sds,
		sizeof(*pool_sds), &pool_sds->checksum, 0, 0);

	int dirty = pool_sds->dirty;

	if (!is_checksum_correct) {
		/* the program was killed during opening or closing the pool */
		CORE_LOG_WARNING(
			"incorrect checksum - SDS will be reinitialized");
		shutdown_state_reinit(curr_sds, pool_sds, rep);
		return 0;
	}

	if (is_uuid_usc_correct) {
		if (dirty == 0)
			return 0;
		/*
		 * the program was killed when the pool was opened
		 * but there wasn't an ADR failure
		 */
		CORE_LOG_WARNING(
			"the pool was not closed - SDS will be reinitialized");
		shutdown_state_reinit(curr_sds, pool_sds, rep);
		return 0;
	}
	if (dirty == 0) {
		/* an ADR failure but the pool was closed */
		CORE_LOG_WARNING(
			"an ADR failure was detected but the pool was closed - SDS will be reinitialized");
		shutdown_state_reinit(curr_sds, pool_sds, rep);
		return 0;
	}
	/* an ADR failure - the pool might be corrupted */
	ERR_WO_ERRNO(
		"an ADR failure was detected, the pool might be corrupted");
	return 1;
}
