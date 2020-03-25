// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2020, Intel Corporation */

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

#define MAX_SIZE_LENGTH 64
#define DAX_REGION_ID_LEN 6 /* 5 digits + \0 */

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
		ERR("file type 0%o not supported", st->st_mode & S_IFMT);
		return PMEM2_E_INVALID_FILE_TYPE;
	}

	char spath[PATH_MAX];
	int ret = util_snprintf(spath, PATH_MAX,
			"/sys/dev/char/%u:%u/subsystem",
			os_major(st->st_rdev), os_minor(st->st_rdev));

	if (ret < 0) {
		/* impossible */
		ERR("!snprintf");
		ASSERTinfo(0, "snprintf failed");
		return PMEM2_E_ERRNO;
	}

	LOG(4, "device subsystem path \"%s\"", spath);

	char npath[PATH_MAX];
	char *rpath = realpath(spath, npath);
	if (rpath == NULL) {
		ERR("!realpath \"%s\"", spath);
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

/*
 * pmem2_device_dax_size_from_stat -- checks the size of a given
 * dax device from given stat structure
 */
int
pmem2_device_dax_size_from_stat(const os_stat_t *st, size_t *size)
{
	char spath[PATH_MAX];
	int ret = util_snprintf(spath, PATH_MAX, "/sys/dev/char/%u:%u/size",
		os_major(st->st_rdev), os_minor(st->st_rdev));

	if (ret < 0) {
		/* impossible */
		ERR("!snprintf");
		ASSERTinfo(0, "snprintf failed");
		return PMEM2_E_ERRNO;
	}

	LOG(4, "device size path \"%s\"", spath);

	int fd = os_open(spath, O_RDONLY);
	if (fd < 0) {
		ERR("!open \"%s\"", spath);
		return PMEM2_E_ERRNO;
	}

	char sizebuf[MAX_SIZE_LENGTH + 1];

	ssize_t nread = read(fd, sizebuf, MAX_SIZE_LENGTH);
	if (nread < 0) {
		ERR("!read");
		int ret = PMEM2_E_ERRNO;
		(void) os_close(fd);
		return ret;
	}
	int olderrno = errno;
	(void) os_close(fd);

	sizebuf[nread] = 0; /* null termination */

	char *endptr;

	errno = 0;

	unsigned long long tmpsize;
	tmpsize = strtoull(sizebuf, &endptr, 0);
	if (endptr == sizebuf || *endptr != '\n') {
		ERR("invalid device size format '%s'", sizebuf);
		errno = olderrno;
		return PMEM2_E_INVALID_SIZE_FORMAT;
	}

	if (tmpsize == ULLONG_MAX && errno == ERANGE) {
		ret = PMEM2_E_ERRNO;
		ERR("invalid device size '%s'", sizebuf);
		errno = olderrno;
		return ret;
	}

	errno = olderrno;

	*size = tmpsize;
	LOG(4, "device size %zu", *size);
	return 0;
}

/*
 * pmem2_device_dax_alignment_from_stat -- checks the alignment of a given
 * dax device from given stat structure
 */
int
pmem2_device_dax_alignment_from_stat(const os_stat_t *st, size_t *alignment)
{
	char spath[PATH_MAX];
	size_t size = 0;
	char *daxpath;
	int olderrno;
	int ret = 0;

	int ret_snprintf = util_snprintf(spath, PATH_MAX, "/sys/dev/char/%u:%u",
		os_major(st->st_rdev), os_minor(st->st_rdev));
	if (ret_snprintf < 0) {
		ERR("!snprintf");
		ASSERTinfo(0, "snprintf failed");
		return PMEM2_E_ERRNO;
	}

	daxpath = realpath(spath, NULL);
	if (!daxpath) {
		ERR("!realpath \"%s\"", spath);
		return PMEM2_E_ERRNO;
	}

	if (util_safe_strcpy(spath, daxpath, sizeof(spath))) {
		ERR("util_safe_strcpy failed");
		free(daxpath);
		return -EINVAL;
	}

	free(daxpath);

	while (spath[0] != '\0') {
		char sizebuf[MAX_SIZE_LENGTH + 1];
		char *pos = strrchr(spath, '/');
		char *endptr;
		size_t len;
		ssize_t rc;
		int fd;

		if (strcmp(spath, "/sys/devices") == 0)
			break;

		if (!pos)
			break;

		*pos = '\0';
		len = strlen(spath);

		ret_snprintf = util_snprintf(&spath[len], sizeof(spath) - len,
			"/dax_region/align");
		if (ret_snprintf < 0) {
			ERR("!snprintf");
			ASSERTinfo(0, "snprintf failed");
			return PMEM2_E_ERRNO;
		}

		fd = os_open(spath, O_RDONLY);
		*pos = '\0';

		if (fd < 0)
			continue;

		LOG(4, "device align path \"%s\"", spath);

		rc = read(fd, sizebuf, MAX_SIZE_LENGTH);
		if (rc < 0) {
			ERR("!read");
			ret = PMEM2_E_ERRNO;
			os_close(fd);
			return ret;
		}

		os_close(fd);

		sizebuf[rc] = 0; /* null termination */

		olderrno = errno;
		errno = 0;

		/* 'align' is in decimal format */
		size = strtoull(sizebuf, &endptr, 10);
		if (endptr == sizebuf || *endptr != '\n') {
			ERR("invalid device alignment format %s", sizebuf);
			errno = olderrno;
			return PMEM2_E_INVALID_ALIGNMENT_FORMAT;
		}

		if (size == ULLONG_MAX && errno == ERANGE) {
			ret = PMEM2_E_ERRNO;
			ERR("invalid device alignment %s", sizebuf);
			errno = olderrno;
			return ret;
		}

		/*
		 * If the alignment value is not a power of two, try with
		 * hex format, as this is how it was printed in older kernels.
		 * Just in case someone is using kernel <4.9.
		 */
		if ((size & (size - 1)) != 0) {
			size = strtoull(sizebuf, &endptr, 16);
			if (endptr == sizebuf || *endptr != '\n') {
				ERR("invalid device alignment format %s",
						sizebuf);
				errno = olderrno;
				return PMEM2_E_INVALID_ALIGNMENT_FORMAT;
			}

			if (size == ULLONG_MAX && errno == ERANGE) {
				ret = PMEM2_E_ERRNO;
				ERR("invalid device alignment %s", sizebuf);
				errno = olderrno;
				return ret;
			}
		}

		errno = olderrno;
		break;
	}

	if (!ret) {
		*alignment = size;
		LOG(4, "device alignment %zu", *alignment);
	}

	return ret;
}

/*
 * pmem2_device_davx_region_find -- returns Device DAX region id
 */
int
pmem2_device_dax_region_find(const os_stat_t *st)
{
	LOG(3, "st %p", st);

	int dax_reg_id_fd;
	char dax_region_path[PATH_MAX];
	char reg_id[DAX_REGION_ID_LEN];
	char *end_addr;

	dev_t dev_id = st->st_rdev;

	unsigned major = os_major(dev_id);
	unsigned minor = os_minor(dev_id);
	int ret = util_snprintf(dax_region_path, PATH_MAX,
		"/sys/dev/char/%u:%u/device/dax_region/id",
		major, minor);
	if (ret < 0) {
		ERR("!snprintf");
		return -1;
	}

	if ((dax_reg_id_fd = os_open(dax_region_path, O_RDONLY)) < 0) {
		LOG(1, "!open(\"%s\", O_RDONLY)", dax_region_path);
		return -1;
	}

	ssize_t len = read(dax_reg_id_fd, reg_id, DAX_REGION_ID_LEN);

	if (len == -1) {
		ERR("!read(%d, %p, %d)", dax_reg_id_fd,
			reg_id, DAX_REGION_ID_LEN);
		goto err;
	} else if (len < 2 || reg_id[len - 1] != '\n') {
		errno = EINVAL;
		ERR("!read(%d, %p, %d) invalid format", dax_reg_id_fd,
			reg_id, DAX_REGION_ID_LEN);
		goto err;
	}

	int olderrno = errno;
	errno = 0;
	long reg_num = strtol(reg_id, &end_addr, 10);
	if ((errno == ERANGE && (reg_num == LONG_MAX || reg_num == LONG_MIN)) ||
			(errno != 0 && reg_num == 0)) {
		ERR("!strtol(%p, %p, 10)", reg_id, end_addr);
		goto err;
	}
	errno = olderrno;

	if (end_addr == reg_id) {
		ERR("!strtol(%p, %p, 10) no digits were found",
			reg_id, end_addr);
		goto err;
	}
	if (*end_addr != '\n') {
		ERR("!strtol(%s, %s, 10) invalid format",
			reg_id, end_addr);
		goto err;
	}

	os_close(dax_reg_id_fd);
	return (int)reg_num;

err:
	os_close(dax_reg_id_fd);
	return -1;
}
