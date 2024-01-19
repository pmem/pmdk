// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2024, Intel Corporation */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include "libpmem2.h"
#include "out.h"
#include "pmem2_utils.h"
#include "region_namespace.h"
#include "source.h"

#define SUBSYSTEM_STR_FORMAT "/sys/dev/char/%u:%u/subsystem"
/*
 * Both major and minor are 32-bit unsigned numbers. The length of
 * the decimal representation of the biggest 32-bit unsigned number is 10.
 */
#define SUBSYSTEM_STR_MAX_SIZE (sizeof(SUBSYSTEM_STR_FORMAT) + 2 * 10)

/*
 * pmem2_get_type_from_stat -- determine type of file based on output of stat
 * syscall
 */
int
pmem2_get_type_from_stat(const os_stat_t *st, enum pmem2_file_type *type)
{
	if (S_ISREG(st->st_mode)) {
		*type = PMEM2_FTYPE_REG;
		return 0;
	}

	if (S_ISDIR(st->st_mode)) {
		*type = PMEM2_FTYPE_DIR;
		return 0;
	}

	if (!S_ISCHR(st->st_mode)) {
		ERR_WO_ERRNO("file type 0%o not supported",
			st->st_mode & S_IFMT);
		return PMEM2_E_INVALID_FILE_TYPE;
	}

	char spath[SUBSYSTEM_STR_MAX_SIZE];
	int ret = util_snprintf(spath, SUBSYSTEM_STR_MAX_SIZE,
			SUBSYSTEM_STR_FORMAT, os_major(st->st_rdev),
			os_minor(st->st_rdev));
	if (ret < 0) {
		/* impossible */
		ERR_W_ERRNO("snprintf");
		ASSERTinfo(0, "snprintf failed");
		return PMEM2_E_ERRNO;
	}

	LOG(4, "device subsystem path \"%s\"", spath);

	char npath[PATH_MAX];
	char *rpath = realpath(spath, npath);
	if (rpath == NULL) {
		ERR_W_ERRNO("realpath \"%s\"", spath);
		return PMEM2_E_ERRNO;
	}

	char *basename = strrchr(rpath, '/');
	if (!basename || strcmp("dax", basename + 1) != 0) {
		LOG(3, "%s path does not match device dax prefix path", rpath);
		return PMEM2_E_INVALID_FILE_TYPE;
	}

	*type = PMEM2_FTYPE_DEVDAX;

	return 0;
}

#undef SUBSYSTEM_STR_MAX_SIZE
#undef SUBSYSTEM_STR_FORMAT
