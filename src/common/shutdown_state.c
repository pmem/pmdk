/*
 * Copyright 2017, Intel Corporation
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
 * shutdown_state.c -- XXX
 */

#include <string.h>
#include <stdbool.h>
#include "shutdown_state.h"
#include "os_dimm.h"
#include "out.h"
#include "util.h"
#include <libpmem.h> /* XXX: deepflush from common */
/* XXX: deep flush */
#define FLUSH_SDS(sds) pmem_persist(sds, sizeof(*sds))

/*
 * pmem_shutdown_state_checksum -- (internal) counts SDS checksum and flush it
 */
static void
shutdown_state_checksum(struct shutdown_state *sds)
{
	util_checksum(sds, sizeof(*sds), &sds->checksum, 1);
	FLUSH_SDS(sds);
}
/*
 * pmem_init_shutdown_state -- initializes shutdown_state struct
 */
int
shutdown_state_init(struct shutdown_state *sds)
{
	memset(sds, 0, sizeof(*sds));
	sds->flag = 0;

	shutdown_state_checksum(sds);

	return 0;
}

/*
 * shutdown_state_add_part -- adds file uuid and usc to shutdown_state struct
 */
int
shutdown_state_add_part(struct shutdown_state *sds, const char *path)
{
	size_t len;
	char *uid;
	uint64_t usc;

	if (os_dimm_usc(path, &usc)) {
		ERR("cannot read unsafe shutdown count of %s", path);
		return 1;
	}

	if (os_dimm_uid_size(path, &len)) {
		ERR("cannot read uuid of %s", path);
		return 1;
	}

	len += 4 - len % 4;

	uid = Zalloc(len);

	if (uid == NULL) {
		ERR("!Zalloc");
		return 1;
	}

	if (os_dimm_uid(path, uid)) {
		ERR("cannot read uuid of %s", path);
		free(uid);
		return 1;
	}

	sds->usc += usc;

	uint64_t tmp;
	util_checksum(uid, len, &tmp, 1);
	sds->uuid += tmp;

	FLUSH_SDS(sds);
	Free(uid);
	shutdown_state_checksum(sds);
	return 0;
}

/*
 * shutdown_state_set_flag -- sets dirty pool flag
 */
void
shutdown_state_set_flag(struct shutdown_state *sds)
{
	sds->flag = 1;
	FLUSH_SDS(sds);

	shutdown_state_checksum(sds);
}

/*
 * pmem_shutdown_clear_flag -- clears dirty pool flag
 */
void
shutdown_state_clear_flag(struct shutdown_state *sds)
{
	sds->flag = 0;
	FLUSH_SDS(sds);

	shutdown_state_checksum(sds);
}

/*
 * pmem_reinit_shutdown_state -- (internal) reinitializes shutdown_state struct
 */
static void
shutdown_state_reinit(struct shutdown_state *curr_sds,
			struct shutdown_state *pool_sds)
{
	shutdown_state_init((struct shutdown_state *)pool_sds);
	pool_sds->uuid = curr_sds->uuid;
	pool_sds->usc = curr_sds->usc;
	pool_sds->flag = 0;

	FLUSH_SDS(pool_sds);

	shutdown_state_checksum(pool_sds);
}

/*
 * shutdown_state_check -- compares and fixes shutdown state
 */
int
shutdown_state_check(struct shutdown_state *curr_sds,
	struct shutdown_state *pool_sds)
{

	struct shutdown_state null_sds;
	memset(&null_sds, 0, sizeof(null_sds));
	if (memcmp(&null_sds, pool_sds, sizeof(null_sds)) == 0) {
		shutdown_state_reinit(curr_sds, pool_sds);
		return 0;
	}

	bool is_uuid_usc_correct = pool_sds->usc == curr_sds->usc &&
		pool_sds->uuid == curr_sds->uuid;

	bool is_checksum_correct = util_checksum(pool_sds,
		sizeof(*pool_sds), &pool_sds->checksum, 0);

	int flag = pool_sds->flag;

	if (!is_checksum_correct) {
		/* the program was killed during opening or closing the pool */
		LOG(3, "incorrect checksum - SDS will be reinitialized");
		shutdown_state_reinit(curr_sds, pool_sds);
		return 0;
	}

	if (is_uuid_usc_correct) {
		if (flag == 0) {
			return 0;
		} else {
			/*
			 * the program was killed when the pool was opened
			 * but there wasn't an adr failure
			 */
			LOG(3,
				"the pool was not closed - SDS will be reinitialized");
			shutdown_state_reinit(curr_sds, pool_sds);
			return 0;
		}
	} else {
		if (flag == 0) {
			/* an adr failure but the pool was closed */
			LOG(3,
				"an ADR failure was detected but the pool was closed - SDS will be reinitialized");
			shutdown_state_reinit(curr_sds, pool_sds);
			return 0;
		} else {
			/* an adr failure - the pool might be corrupted */
			ERR(
				"an ADR failure was detected, the pool might be corrupted");
			return 1;
		}
	}
}
