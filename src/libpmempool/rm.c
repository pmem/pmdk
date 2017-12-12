/*
 * Copyright 2016-2017, Intel Corporation
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
#include <fcntl.h>

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

struct cb_args {
	int flags;
	int error;
};

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
	if (!Rpmem_remove) {
		ERR_F(flags, "cannot remove remote replica"
			" -- missing librpmem");
		return -1;
	}

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
	struct cb_args *args = (struct cb_args *)arg;
	int ret;
	if (pf->is_remote) {
		ret = rm_remote(pf->node_addr, pf->pool_desc, args->flags);
	} else {
		ret = rm_local(pf->path, args->flags, 1);
	}

	if (ret)
		args->error = ret;

	return 0;
}

/*
 * pmempool_rmU -- remove pool files or poolsets
 */
#ifndef _WIN32
static inline
#endif
int
pmempool_rmU(const char *path, int flags)
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

	/* fill up pool_set structure */
	struct pool_set *set = NULL;
	int fd = os_open(path, O_RDONLY);
	if (fd == -1 || util_poolset_parse(&set, path, fd)) {
		ERR_F(flags, "parsing poolset file failed");
		if (fd != -1)
			os_close(fd);
		if (CHECK_FLAG(flags, FORCE))
			return 0;
		return -1;
	}
	os_close(fd);

	if (set->remote) {
		/* ignore error - it will be handled in rm_remote() */
		(void) util_remote_load();
	}

	util_poolset_free(set);

	struct cb_args args;
	args.flags = flags;
	args.error = 0;
	ret = util_poolset_foreach_part(path, rm_cb, &args);
	if (ret == -1) {
		ERR_F(flags, "parsing poolset file failed");
		if (CHECK_FLAG(flags, FORCE))
			return 0;

		return ret;
	}

	ASSERTeq(ret, 0);

	if (args.error)
		return args.error;

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

#ifndef _WIN32
/*
 * pmempool_rm -- remove pool files or poolsets
 */
int
pmempool_rm(const char *path, int flags)
{
	return pmempool_rmU(path, flags);
}
#else
/*
 * pmempool_rmW -- remove pool files or poolsets in widechar
 */
int
pmempool_rmW(const wchar_t *path, int flags)
{
	char *upath = util_toUTF8(path);
	if (upath == NULL) {
		ERR("Invalid poolest/pool file path.");
		return -1;
	}

	int ret = pmempool_rmU(upath, flags);

	util_free_UTF8(upath);
	return ret;
}
#endif
