/*
 * Copyright 2014-2019, Intel Corporation
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

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include "libpmem2.h"
#include "out.h"
#include "pmem2_utils_posix.h"

#define MAX_SIZE_LENGTH 64

int
pmem2_get_type_from_stat(const os_stat_t *st, enum pmem2_file_type *type)
{
	if (S_ISREG(st->st_mode)) {
		*type = FTYPE_REG;
		return 0;
	}

	if (S_ISDIR(st->st_mode)) {
		*type = FTYPE_DIR;
		return 0;
	}

	if (!S_ISCHR(st->st_mode)) {
		ERR("file type 0%o not supported", st->st_mode & S_IFMT);
		errno = EINVAL;
		return PMEM2_E_INVALID_FILE_TYPE;
	}

	char spath[PATH_MAX];
	int ret = snprintf(spath, PATH_MAX, "/sys/dev/char/%u:%u/subsystem",
		os_major(st->st_rdev), os_minor(st->st_rdev));

	if (ret < 0) {
		/* impossible */
		ERR("!snprintf");
		ASSERTinfo(0, "snprintf failed");
		return PMEM2_E_UNKNOWN;
	}

	if (ret >= PATH_MAX) {
		/* impossible */
		ERR("BUG: too short buffer for sysfs path");
		ASSERTinfo(0, "too short buffer for sysfs path");
		errno = EINVAL;
		return PMEM2_E_UNKNOWN;
	}

	LOG(4, "device subsystem path \"%s\"", spath);

	char npath[PATH_MAX];
	char *rpath = realpath(spath, npath);
	if (rpath == NULL) {
		ERR("!realpath \"%s\"", spath);
		return pmem2_errno_to_err();
	}

	char *basename = strrchr(rpath, '/');
	if (!basename || strcmp("dax", basename + 1) != 0) {
		LOG(3, "%s path does not match device dax prefix path", rpath);
		errno = EINVAL;
		return PMEM2_E_INVALID_FILE_TYPE;
	}

	*type = FTYPE_DEVDAX;

	return 0;
}

/*
 * pmem2_device_dax_size_from_stat -- (internal) checks the size of a given
 * dax device from given stat structure
 */
int
pmem2_device_dax_size_from_stat(const os_stat_t *st, ssize_t *size)
{
	char spath[PATH_MAX];
	int ret = snprintf(spath, PATH_MAX, "/sys/dev/char/%u:%u/size",
		os_major(st->st_rdev), os_minor(st->st_rdev));

	if (ret < 0) {
		/* impossible */
		ERR("!snprintf");
		ASSERTinfo(0, "snprintf failed");
		return PMEM2_E_UNKNOWN;
	}

	if (ret >= PATH_MAX) {
		/* impossible */
		ERR("BUG: too short buffer for sysfs path");
		ASSERTinfo(0, "too short buffer for sysfs path");
		errno = EINVAL;
		return PMEM2_E_UNKNOWN;
	}


	LOG(4, "device size path \"%s\"", spath);

	int fd = os_open(spath, O_RDONLY);
	if (fd < 0) {
		ERR("!open \"%s\"", spath);
		return pmem2_errno_to_err();
	}

	char sizebuf[MAX_SIZE_LENGTH + 1];

	ssize_t nread = read(fd, sizebuf, MAX_SIZE_LENGTH);
	if (nread < 0) {
		ERR("!read");
		int ret = pmem2_errno_to_err();
		int olderrno = errno;
		(void) os_close(fd);
		errno = olderrno;
		return ret;
	}
	int olderrno = errno;
	(void) os_close(fd);
	errno = olderrno;

	sizebuf[nread] = 0; /* null termination */

	char *endptr;

	errno = 0;

	*size = strtoll(sizebuf, &endptr, 0);
	if (endptr == sizebuf || *endptr != '\n' ||
	    ((*size == LLONG_MAX || *size == LLONG_MIN) && errno == ERANGE)) {
		ERR("invalid device size %s", sizebuf);
		return pmem2_errno_to_err();
	}

	errno = olderrno;

	LOG(4, "device size %zu", *size);
	return 0;
}
