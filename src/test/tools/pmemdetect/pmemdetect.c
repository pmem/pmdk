/*
 * Copyright 2016-2018, Intel Corporation
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
 * pmemdetect.c -- detect PMEM/Device DAX device or Device DAX alignment
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>

#include "mmap.h"
#include "libpmem.h"
#include "file.h"
#include "os.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#define SIZE 4096

#define DEVDAX_DETECT		(1 << 0)
#define DEVDAX_ALIGN		(1 << 1)
#define MAP_SYNC_SUPP		(1 << 2)
#define DAX_REGION_DETECT	(1 << 3)
#define FILE_SIZE		(1 << 4)

#define err(fmt, ...) fprintf(stderr, "pmemdetect: " fmt, __VA_ARGS__)

/* arguments */
static int Opts;
static char *Path;
static size_t Align;

/*
 * print_usage -- print short description of usage
 */
static void
print_usage(void)
{
	printf("Usage: pmemdetect [options] <path>\n");
	printf("Valid options:\n");
	printf("-d, --devdax      - check if <path> is Device DAX\n");
	printf("-a, --align=N     - check Device DAX alignment\n");
	printf("-r, --dax-region  - check if Dev DAX <path> has region id\n");
	printf("-s, --map-sync    - check if <path> supports MAP_SYNC\n");
	printf("-z, --size        - print file/Device DAX size\n");
	printf("-h, --help        - print this usage info\n");
}

/*
 * long_options -- command line options
 */
static const struct option long_options[] = {
	{"devdax",	no_argument,		NULL,	'd'},
	{"align",	required_argument,	NULL,	'a'},
	{"dax-region",	no_argument,		NULL,	'r'},
	{"map-sync",	no_argument,		NULL,	's'},
	{"size",	no_argument,		NULL,	'z'},
	{"help",	no_argument,		NULL,	'h'},
	{NULL,		0,			NULL,	 0 },
};

/*
 * parse_args -- (internal) parse command line arguments
 */
static int
parse_args(int argc, char *argv[])
{
	int opt;
	while ((opt = getopt_long(argc, argv, "a:dshrz",
			long_options, NULL)) != -1) {
		switch (opt) {
		case 'd':
			Opts |= DEVDAX_DETECT;
			break;
		case 'r':
			Opts |= DAX_REGION_DETECT;
			break;
		case 'a':
			Opts |= DEVDAX_ALIGN;
			char *endptr;
			errno = 0;
			size_t align = strtoull(optarg, &endptr, 0);
			if ((endptr && *endptr != '\0') || errno) {
				err("'%s' -- invalid alignment", optarg);
				return -1;
			}
			Align = align;
			break;
		case 's':
			Opts |= MAP_SYNC_SUPP;
			break;
		case 'z':
			Opts |= FILE_SIZE;
			break;
		case 'h':
			print_usage();
			exit(EXIT_SUCCESS);
		default:
			print_usage();
			exit(EXIT_FAILURE);
		}
	}

	if (optind < argc) {
		Path = argv[optind];
	} else {
		print_usage();
		exit(EXIT_FAILURE);
	}

	return 0;
}

/*
 * get_params -- get parameters for pmem_map_file
 */
static int
get_params(const char *path, int *flags, size_t *size)
{
	int ret;

	os_stat_t buf;
	ret = os_stat(path, &buf);
	if (ret && errno != ENOENT) {
		/* error other than no such file */
		perror(path);
		return -1;
	}

	if (ret) {
		/* no such file */
		*flags = PMEM_FILE_CREATE;
		*size = SIZE;
	} else if (S_ISDIR(buf.st_mode)) {
		*flags = PMEM_FILE_CREATE | PMEM_FILE_TMPFILE;
		*size = SIZE;
	} else {
		/* file exist */
		*size = 0;
		*flags = 0;
	}

	return 0;
}

/*
 * is_pmem -- checks if given path points to pmem-aware filesystem
 */
static int
is_pmem(const char *path)
{
	int ret;
	int flags;
	size_t size;

	ret = get_params(path, &flags, &size);
	if (ret)
		return ret;

	int is_pmem;
	void *addr = pmem_map_file(path, size, flags, 0, &size, &is_pmem);
	if (addr == NULL) {
		perror("pmem_map_file failed");
		return -1;
	}

	pmem_unmap(addr, size);

	return is_pmem;
}

/*
 * is_dev_dax -- checks if given path points to Device DAX
 */
static int
is_dev_dax(const char *path)
{
	if (os_access(path, W_OK|R_OK)) {
		printf("%s -- permission denied\n", path);
		return -1;
	}

	if (!util_file_is_device_dax(path)) {
		printf("%s -- not device dax\n", path);
		return 0;
	}

	return 1;
}

/*
 * is_dev_dax_align -- checks if Device DAX alignment is as specified
 */
static int
is_dev_dax_align(const char *path, size_t req_align)
{
	if (is_dev_dax(path) != 1)
		return -1;

	size_t align = util_file_device_dax_alignment(path);
	return (req_align == align) ? 1 : 0;
}

/*
 * supports_map_sync -- checks if MAP_SYNC is supported on a filesystem
 * from given path
 */
static int
supports_map_sync(const char *path)
{
	int ret;
	int flags;
	size_t size;

	ret = get_params(path, &flags, &size);
	if (ret)
		return ret;

	int fd;
	if (flags & PMEM_FILE_TMPFILE)
		fd = util_tmpfile(path, "/pmemdetect.XXXXXX", 0);
	else if (flags & PMEM_FILE_CREATE)
		fd = os_open(path, O_CREAT|O_RDWR, S_IWUSR|S_IRUSR);
	else
		fd = os_open(path, O_RDWR);

	if (fd < 0) {
		perror(path);
		return -1;
	}

	if (flags & PMEM_FILE_CREATE) {
		ret = os_ftruncate(fd, (off_t)size);
		if (ret) {
			perror(path);
			os_close(fd);
			return -1;
		}
	}

	void *addr = mmap(NULL, size, PROT_READ|PROT_WRITE,
		MAP_SHARED|MAP_SYNC|MAP_SHARED_VALIDATE, fd, 0);

	if (addr != MAP_FAILED) {
		ret = 1;
	} else if (addr == MAP_FAILED &&
		(errno == EOPNOTSUPP || errno == EINVAL)) {
		ret = 0;
	} else {
		err("mmap: %s\n", strerror(errno));
		ret = -1;
	}

	os_close(fd);

	if (flags & PMEM_FILE_CREATE && !(flags & PMEM_FILE_TMPFILE))
		util_unlink(path);


	return ret;
}

int
main(int argc, char *argv[])
{
#ifdef _WIN32
	wchar_t **wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
	for (int i = 0; i < argc; i++) {
		argv[i] = util_toUTF8(wargv[i]);
		if (argv[i] == NULL) {
			for (i--; i >= 0; i--)
				free(argv[i]);
			err("error during arguments conversion\n");
			return 2;
		}
	}
#endif
	int ret;

	if (parse_args(argc, argv)) {
		ret = 2;
		goto out;
	}

	util_init();
	util_mmap_init();

	if (Opts & DEVDAX_DETECT)
		ret = is_dev_dax(Path);
	else if (Opts & DAX_REGION_DETECT) {
		ret = util_ddax_region_find(Path);
		if (ret < 0) {
			printf("Sysfs id file for dax_region is not supported:"
				" %s\n", Path);
			ret = 0;
		} else {
			ret = 1;
		}
	} else if (Opts & DEVDAX_ALIGN) {
		ret = is_dev_dax_align(Path, Align);
	} else if (Opts & FILE_SIZE) {
		printf("%zu", (size_t)util_file_get_size(Path));
		ret = 1;
	} else if (Opts & MAP_SYNC_SUPP) {
		ret = supports_map_sync(Path);
	} else {
		ret = is_pmem(Path);
	}

	/*
	 * Return 0 on 'true'. Otherwise return 1.
	 * If any problem occurred return 2.
	 */
	switch (ret) {
	case 0:
		ret = 1;
		break;
	case 1:
		ret = 0;
		break;
	default:
		ret = 2;
		break;
	}

	util_mmap_fini();
out:
#ifdef _WIN32
	for (int i = argc; i > 0; i--)
		free(argv[i - 1]);
#endif
	return ret;
}
