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
 * util_sysfs.c -- unit test for sysfs iterators
 *
 */

#include "unittest.h"
#include "sysfs.h"

#define FSNAME_LEN 255

static void
test_sysfs_iter(void)
{
	struct sysfs_iter *iter = sysfs_new("/proc/filesystems", "%s");

	char fsname[FSNAME_LEN];
	int n = 0;
	int ext4 = 0;

	while (sysfs_next(iter, fsname) >= 0) {
		if (strcmp("nodev", fsname) == 0)
			continue;

		if (strcmp("ext4", fsname) == 0)
			ext4++;

		n++;
	}

	UT_ASSERTeq(ext4, 1);
	UT_ASSERTne(n, 0);

	sysfs_delete(iter);
}

static void
test_sysfs_single(void)
{
	char linuxstr[sizeof("Linux ")];

	int ret = sysfs_single("/proc/version", "%s", linuxstr);
	UT_ASSERTeq(ret, 1);
	UT_ASSERTeq(strcmp("Linux", linuxstr), 0);
}

static void
test_sysfs_dev_single(const char *path)
{
	int fd = OPEN(path, O_RDWR);
	UT_ASSERTne(fd, -1);

	uint64_t sector = 0;
	int ret = sysfs_dev_single(fd, "queue/hw_sector_size", "%llu",
		&sector);
	UT_ASSERTeq(ret, 1);

	UT_ASSERTne(sector, 0);

	CLOSE(fd);
}

static void
test_sysfs_dev(const char *path)
{
	int fd = OPEN(path, O_RDWR);
	UT_ASSERTne(fd, -1);

	struct sysfs_iter *iter =
		sysfs_dev_new(fd, "stat", "%lu");
	UT_ASSERTne(iter, NULL);

	uint64_t stat_value;
	int n = 0;
	while (sysfs_next(iter, &stat_value) >= 0)
		n++;

	/* dev stat file should contain 11 values, but this is safer */
	UT_ASSERTne(n, 0);

	sysfs_delete(iter);
	CLOSE(fd);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "util_sysfs");

	if (argc != 2)
		UT_FATAL("usage: %s file", argv[0]);

	test_sysfs_iter();
	test_sysfs_single();
	test_sysfs_dev_single(argv[1]);
	test_sysfs_dev(argv[1]);

	DONE(NULL);
}
