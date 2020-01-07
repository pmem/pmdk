// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018, Intel Corporation */

/*
 * fallocate_detect -- checks fallocate support on filesystem
 */

#define _GNU_SOURCE
#include "file.h"
#include "os.h"

#ifdef __linux__
#include <errno.h>
#include <fcntl.h>
#include <linux/magic.h>
#include <sys/vfs.h>

/*
 * posix_fallocate on Linux is implemented using fallocate
 * syscall. This syscall requires file system-specific code on
 * the kernel side and not all file systems have this code.
 * So when posix_fallocate gets 'not supported' error from
 * fallocate it falls back to just writing zeroes.
 * Detect it and return information to the caller.
 */
static int
check_fallocate(const char *file)
{
	int exit_code = 0;
	int fd = os_open(file, O_RDWR | O_CREAT | O_EXCL, 0644);
	if (fd < 0) {
		perror("os_open");
		return 2;
	}

	if (fallocate(fd, 0, 0, 4096)) {
		if (errno == EOPNOTSUPP) {
			exit_code = 1;
			goto exit;
		}

		perror("fallocate");
		exit_code = 2;
		goto exit;
	}

	struct statfs fs;
	if (!fstatfs(fd, &fs)) {
		if (fs.f_type != EXT4_SUPER_MAGIC /* also ext2, ext3 */) {
			/*
			 * On CoW filesystems, fallocate reserves _amount
			 * of_ space but doesn't allocate a specific block.
			 * As we're interested in DAX filesystems only, just
			 * skip these tests anywhere else.
			 */
			exit_code = 1;
			goto exit;
		}
	}

exit:
	os_close(fd);
	os_unlink(file);

	return exit_code;
}
#else
/* no support for fallocate in FreeBSD */
static int
check_fallocate(const char *file)
{
	return 1;
}
#endif

int
main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s filename\n", argv[0]);
		return 1;
	}

	return check_fallocate(argv[1]);
}
