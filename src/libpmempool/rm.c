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
 * rm.c -- implementation of pmempool_rm() function
 */
#include <errno.h>

#include "libpmempool.h"
#include "out.h"
#include "os.h"
#include "util.h"
#include "set.h"
#include "file.h"

#define PMEMPOOL_RM_ALL_FLAGS (\
	PMEMPOOL_RM_FORCE |\
	PMEMPOOL_RM_POOLSET_LOCAL |\
	PMEMPOOL_RM_POOLSET_REMOTE)

#define ERR_F(f, ...) do {\
	if (CHECK_FLAG((f), FORCE))\
		LOG(2, "!(ignored) " __VA_ARGS__);\
	else\
		ERR(__VA_ARGS__);\
} while (0)

#define CHECK_FLAG(f, i) ((f) & PMEMPOOL_RM_##i)

/*
 * rm_local -- (internal) remove single local file
 */
static int
rm_local(const char *path, int flags, int is_part_file)
{
	int ret = util_unlink_flock(path);
	if (!ret) {
		LOG(3, "%s: removed", path);
		return 0;
	}

	int oerrno = errno;
	os_stat_t buff;
	ret = os_stat(path, &buff);
	if (!ret) {
		if (S_ISDIR(buff.st_mode)) {
			errno = EISDIR;
			if (is_part_file)
				ERR("%s: removing file failed", path);
			else
				ERR("removing file failed");
			return -1;
		}
	}

	errno = oerrno;

	if (is_part_file)
		ERR_F(flags, "%s: removing file failed", path);
	else
		ERR_F(flags, "removing file failed");

	if (CHECK_FLAG(flags, FORCE))
		return 0;

	return -1;
}

/*
 * rm_remote -- (internal) remove remote replica
 */
static int
rm_remote(const char *node, const char *path, int flags)
{
	int rpmem_flags = 0;
	if (CHECK_FLAG(flags, FORCE))
		rpmem_flags |= RPMEM_REMOVE_FORCE;

	if (CHECK_FLAG(flags, POOLSET_REMOTE))
		rpmem_flags |= RPMEM_REMOVE_POOL_SET;

	int ret = Rpmem_remove(node, path, rpmem_flags);
	if (ret) {
		ERR_F(flags, "%s/%s removing failed", node, path);
		if (CHECK_FLAG(flags, FORCE))
			ret = 0;
	} else {
		LOG(3, "%s/%s: removed", node, path);
	}

	return ret;
}

/*
 * rm_cb -- (internal) foreach part callback
 */
static int
rm_cb(struct part_file *pf, void *arg)
{
	int flags = *(int *)arg;
	int ret;
	if (pf->is_remote) {
		ret = rm_remote(pf->node_addr, pf->pool_desc, flags);
	} else {
		ret = rm_local(pf->path, flags, 1);
	}

	/* return 0 (success) or 1 (error) */
	return !!ret;
}

#ifdef _WIN32
/*
 * pmempool_rmW -- remove pool files or poolsets in widechar
 */
int
pmempool_rmW(const wchar_t *path, int flags)
{
	char *_path = util_toUTF8(path);
	if (_path == NULL) {
		ERR("Invalid poolest/pool file path.");
		return -1;
	}

	int ret = pmempool_rmU(_path, flags);

	Free(_path);
	return ret;
}
#endif

/*
 * pmempool_rm -- remove pool files or poolsets
 */
int
UNICODE_FUNCTION(pmempool_rm)(const char *path, int flags)
{
	LOG(3, "path %s flags %x", path, flags);
	int ret;

	if (flags & ~PMEMPOOL_RM_ALL_FLAGS) {
		ERR("invalid flags specified");
		errno = EINVAL;
		return -1;
	}

	int is_poolset = util_is_poolset_file(path);
	if (is_poolset < 0) {
		os_stat_t buff;
		ret = os_stat(path, &buff);
		if (!ret) {
			if (S_ISDIR(buff.st_mode)) {
				errno = EISDIR;
				ERR("removing file failed");
				return -1;
			}
		}
		ERR_F(flags, "removing file failed");
		if (CHECK_FLAG(flags, FORCE))
			return 0;

		return -1;
	}

	if (!is_poolset) {
		LOG(2, "%s: not a poolset file", path);
		return rm_local(path, flags, 0);
	}

	LOG(2, "%s: poolset file", path);
	ret = util_poolset_foreach_part(path, rm_cb, &flags);
	if (ret == -1) {
		ERR_F(flags, "parsing poolset file failed");
		if (CHECK_FLAG(flags, FORCE))
			return 0;

		return ret;
	}

	if (ret == 1) {
		ASSERTeq(CHECK_FLAG(flags, FORCE), 0);
		return ret;
	}

	if (CHECK_FLAG(flags, POOLSET_LOCAL)) {
		ret = rm_local(path, flags, 0);
		if (ret) {
			ERR_F(flags, "removing pool set file failed");
		} else {
			LOG(3, "%s: removed", path);
		}

		if (CHECK_FLAG(flags, FORCE))
			return 0;

		return ret;
	}

	return 0;
}
