/*
 * Copyright 2017-2019, Intel Corporation
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
 * shutdown_state.c -- unsafe shudown detection
 */

#include <string.h>
#include <stdbool.h>
#include <endian.h>
#include "shutdown_state.h"
#include "os_dimm.h"
#include "out.h"
#include "util.h"
#include "os_deep.h"
#include "set.h"

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
shutdown_state_add_part(struct shutdown_state *sds, const char *path,
	struct pool_replica *rep)
{
	LOG(3, "sds %p, path %s", sds, path);

	size_t len = 0;
	char *uid;
	uint64_t usc;

	if (os_dimm_usc(path, &usc)) {
		LOG(2, "cannot read unsafe shutdown count for %s", path);
		return 1;
	}

	if (os_dimm_uid(path, NULL, &len)) {
		ERR("cannot read uuid of %s", path);
		return 1;
	}

	len += 4 - len % 4;
	uid = Zalloc(len);

	if (uid == NULL) {
		ERR("!Zalloc");
		return 1;
	}

	if (os_dimm_uid(path, uid, &len)) {
		ERR("cannot read uuid of %s", path);
		Free(uid);
		return 1;
	}

	sds->usc = htole64(le64toh(sds->usc) + usc);

	uint64_t tmp;
	util_checksum(uid, len, &tmp, 1, 0);
	sds->uuid = htole64(le64toh(sds->uuid) + tmp);

	FLUSH_SDS(sds, rep);
	Free(uid);
	shutdown_state_checksum(sds, rep);
	return 0;
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
		LOG(2, "incorrect checksum - SDS will be reinitialized");
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
		LOG(2,
			"the pool was not closed - SDS will be reinitialized");
		shutdown_state_reinit(curr_sds, pool_sds, rep);
		return 0;
	}
	if (dirty == 0) {
		/* an ADR failure but the pool was closed */
		LOG(2,
			"an ADR failure was detected but the pool was closed - SDS will be reinitialized");
		shutdown_state_reinit(curr_sds, pool_sds, rep);
		return 0;
	}
	/* an ADR failure - the pool might be corrupted */
	ERR("an ADR failure was detected, the pool might be corrupted");
	return 1;
}
