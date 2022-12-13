// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2022, Intel Corporation */

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
	PMEMPOOL_RM_POOLSET_LOCAL)

#define ERR_F(f, ...) do {\
	if (CHECK_FLAG((f), FORCE))\
		LOG(2, "!(ignored) " __VA_ARGS__);\
	else\
		ERR(__VA_ARGS__);\
} while (0)

#define CHECK_FLAG(f, i) ((f) & PMEMPOOL_RM_##i)

struct cb_args {
	unsigned flags;
	int error;
};

/*
 * rm_local -- (internal) remove single local file
 */
static int
rm_local(const char *path, unsigned flags, int is_part_file)
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
 * rm_cb -- (internal) foreach part callback
 */
static int
rm_cb(struct part_file *pf, void *arg)
{
	struct cb_args *args = (struct cb_args *)arg;
	int ret;
	ret = rm_local(pf->part->path, args->flags, 1);

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
pmempool_rmU(const char *path, unsigned flags)
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
pmempool_rm(const char *path, unsigned flags)
{
	return pmempool_rmU(path, flags);
}
#else
/*
 * pmempool_rmW -- remove pool files or poolsets in widechar
 */
int
pmempool_rmW(const wchar_t *path, unsigned flags)
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
