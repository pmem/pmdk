/*
 * Copyright 2019, Intel Corporation
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
 * pmem2_map.c -- pmem2_map unittests
 */

#include <stdbool.h>

#include "config.h"
#include "map.h"
#include "out.h"
#include "pmem2.h"
#include "unittest.h"
#include "ut_pmem2_utils.h"

#define KILOBYTE (1 << 10)
#define MEGABYTE (1 << 20)

/*
 * get_size -- get a file size
 */
static size_t
get_size(const char *file)
{
	os_stat_t stbuf;
	STAT(file, &stbuf);

	return (size_t)stbuf.st_size;
}

/*
 * prepare_config -- fill pmem2_config
 */
static void
prepare_config(struct pmem2_config *cfg, int *fd, const char *file,
		size_t length, size_t offset, int access)
{
	*fd = OPEN(file, access);

	pmem2_config_init(cfg);
	cfg->offset = offset;
	cfg->length = length;
	cfg->requested_max_granularity = PMEM2_GRANULARITY_PAGE;
#ifdef _WIN32
	cfg->handle = (HANDLE)_get_osfhandle(*fd);
#else
	cfg->fd = *fd;
#endif
}


#ifdef _WIN32

#define HIDWORD(x) ((DWORD)((x) >> 32))
#define LODWORD(x) ((DWORD)((x) & 0xFFFFFFFF))

/*
 * prepare_map -- map accordingly to the config
 *
 * XXX it is assumed pmem2_config contains exact arguments e.g.
 * length won't be altered by the file size.
 */
static void
prepare_map(struct pmem2_map **map_ptr, struct pmem2_config *cfg)
{
	struct pmem2_map *map = malloc(sizeof(*map));
	UT_ASSERTne(map, NULL);

	size_t max_size = cfg->length + cfg->offset;
	HANDLE mh = CreateFileMapping(cfg->handle,
		NULL,
		PAGE_READWRITE,
		HIDWORD(max_size),
		LODWORD(max_size),
		NULL);
	UT_ASSERTne(mh, NULL);
	UT_ASSERTne(GetLastError(), ERROR_ALREADY_EXISTS);

	map->addr = MapViewOfFileEx(mh,
		FILE_MAP_ALL_ACCESS,
		HIDWORD(cfg->offset),
		LODWORD(cfg->offset),
		cfg->length,
		NULL);
	UT_ASSERTne(map->addr, NULL);

	UT_ASSERTne(CloseHandle(mh), 0);

	map->length = cfg->length;
	map->effective_granularity = PMEM2_GRANULARITY_PAGE;

	*map_ptr = map;
}
#else
/*
 * prepare_map -- map accordingly to the config
 *
 * XXX this function currently calls mmap(3) without MAP_SYNC so the only
 * mapping granularity is PMEM2_GRANULARITY_PAGE.
 *
 * XXX it is assumed pmem2_config contains exact mmap(3) arguments e.g.
 * length won't be altered by the file size.
 */
static void
prepare_map(struct pmem2_map **map_ptr, struct pmem2_config *cfg)
{
	int flags = MAP_SHARED;
	int proto = PROT_READ | PROT_WRITE;

	off_t offset = (off_t)cfg->offset;
	UT_ASSERTeq((size_t)offset, cfg->offset);

	struct pmem2_map *map = malloc(sizeof(*map));
	UT_ASSERTne(map, NULL);

	map->addr = mmap(NULL, cfg->length, proto, flags, cfg->fd, offset);
	UT_ASSERTne(map->addr, MAP_FAILED);

	map->length = cfg->length;
	map->effective_granularity = PMEM2_GRANULARITY_PAGE;

	*map_ptr = map;
}
#endif

/*
 * unmap_map -- unmap the mapping according to pmem2_map struct
 */
static void
unmap_map(struct pmem2_map *map)
{
#ifdef _WIN32
	UT_ASSERTne(UnmapViewOfFile(map->addr), 0);
#else
	UT_ASSERTeq(munmap(map->addr, map->length), 0);
#endif
}

/*
 * test_map_rdrw_file - map a O_RDWR file
 */
static int
test_map_rdrw_file(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s test_map_rdrw_file <file>", argv[0]);

	char *file = argv[0];
	struct pmem2_config cfg;
	int fd;
	prepare_config(&cfg, &fd, file, 0, 0, O_RDWR);

	struct pmem2_map *map;
	int ret = pmem2_map(&cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	unmap_map(map);
	FREE(map);
	CLOSE(fd);

	return 1;
}

/*
 * test_map_rdonly_file - map a O_RDONLY file
 */
static int
test_map_rdonly_file(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s test_map_rdonly_file <file>", argv[0]);

	char *file = argv[0];
	struct pmem2_config cfg;
	int fd;
	prepare_config(&cfg, &fd, file, 0, 0, O_RDONLY);

	struct pmem2_map *map;
	int ret = pmem2_map(&cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	unmap_map(map);
	FREE(map);
	CLOSE(fd);

	return 1;
}

/*
 * test_map_wronly_file - map a O_WRONLY file
 */
static int
test_map_wronly_file(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s test_map_wronly_file <file>", argv[0]);

	char *file = argv[0];
	struct pmem2_config cfg;
	int fd;
	prepare_config(&cfg, &fd, file, 0, 0, O_WRONLY);

	struct pmem2_map *map;
	int ret = pmem2_map(&cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, -EACCES);

	CLOSE(fd);

	return 1;
}

/*
 * map_valid_ranges_common -- map valid range and validate its length
 * Includes cleanup.
 */
static void
map_valid_ranges_common(const char *file, size_t offset, size_t length,
		size_t val_length)
{
	struct pmem2_config cfg;
	struct pmem2_map *map;
	int ret = 0;
	int fd;

	prepare_config(&cfg, &fd, file, length, offset, O_RDWR);
	ret = pmem2_map(&cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(map->length, val_length);

	unmap_map(map);
	CLOSE(fd);
}

/*
 * test_map_valid_ranges - map valid memory ranges
 */
static int
test_map_valid_ranges(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s test_map_valid_ranges <file>", argv[0]);

	char *file = argv[0];
	size_t size = get_size(file);
	size_t size2 = size / 2;

	/* the config WITHOUT provided length allows mapping the whole file */
	map_valid_ranges_common(file, 0, 0, size);

	/* the config WITH provided length allows mapping the whole file */
	map_valid_ranges_common(file, 0, size, size);

	/* the config with provided length different than the file length */
	map_valid_ranges_common(file, 0, size2, size2);

	/* verify the config with provided length and a valid offset */
	map_valid_ranges_common(file, 2 * MEGABYTE, size2, size2);

	return 1;
}

/*
 * test_map_invalid_ranges - map invalid memory ranges
 */
static int
test_map_invalid_ranges(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s test_map_invalid_ranges <file>", argv[0]);

	char *file = argv[0];
	struct pmem2_config cfg;
	size_t size = get_size(file);
	struct pmem2_map *map;
	int ret = 0;
	int fd;

	/* the mapping size (unaligned) > the file size */
	prepare_config(&cfg, &fd, file, size + 1, 0, O_RDWR);
	ret = pmem2_map(&cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_MAP_RANGE);
	CLOSE(fd);

	/* the mapping + the offset > the file size */
	UT_ASSERT(size / 2 < 10 * MEGABYTE);
	prepare_config(&cfg, &fd, file, size / 2, 10 * MEGABYTE, O_RDWR);
	ret = pmem2_map(&cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_MAP_RANGE);
	CLOSE(fd);

	/* the mapping size (aligned) > the file size */
	UT_ASSERT(size < 20 * MEGABYTE);
	prepare_config(&cfg, &fd, file, 0, 20 * MEGABYTE, O_RDWR);
	ret = pmem2_map(&cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_MAP_RANGE);
	CLOSE(fd);

	return 1;
}

/*
 * test_map_invalid_alignment - map using invalid alignment in the offset
 */
static int
test_map_invalid_alignment(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s test_map_invalid_args <file>", argv[0]);

	char *file = argv[0];
	struct pmem2_config cfg;
	size_t length = get_size(file) / 2;
	struct pmem2_map *map;
	int fd;

	prepare_config(&cfg, &fd, file, length, KILOBYTE, O_RDWR);
	int ret = pmem2_map(&cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, -EINVAL);
	CLOSE(fd);

	return 1;
}

/*
 * test_map_invalid_fd - map using a invalid file descriptor
 */
static int
test_map_invalid_fd(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s test_map_invalid_args <file>", argv[0]);

	char *file = argv[0];
	struct pmem2_config cfg;
	size_t length = get_size(file) / 2;
	struct pmem2_map *map;
	int fd;

	/* the invalid file descriptor */
	prepare_config(&cfg, &fd, file, length, 0, O_RDWR);
	CLOSE(fd);
	int ret = pmem2_map(&cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_INVALID_FILE_HANDLE);

	return 1;
}

/*
 * test_map_empty_config - map using an empty config
 */
static int
test_map_empty_config(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s test_map_invalid_args <file>", argv[0]);

	struct pmem2_config cfg;
	struct pmem2_map *map;

	pmem2_config_init(&cfg);
	int ret = pmem2_map(&cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_FILE_HANDLE_NOT_SET);

	return 1;
}

/*
 * test_unmap_valid - unmap valid pmem2 mapping
 */
static int
test_unmap_valid(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s test_unmap_valid <file>", argv[0]);

	char *file = argv[0];
	struct pmem2_config cfg;
	struct pmem2_map *map = NULL;
	int fd;

	prepare_config(&cfg, &fd, file, get_size(file), 0, O_RDWR);
	prepare_map(&map, &cfg);

	/* unmap the valid mapping */
	int ret = pmem2_unmap(&map);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(map, NULL);
	CLOSE(fd);

	return 1;
}

typedef void (*spoil_func)(struct pmem2_map *map, bool *unmap_required);

/*
 * unmap_invalid_common - unmap an invalid pmem2 mapping
 */
static int
unmap_invalid_common(const char *file, spoil_func spoil, int exp_ret)
{
	struct pmem2_config cfg;
	struct pmem2_map *map = NULL;
	struct pmem2_map map_copy;
	bool unmap_required = true;
	int fd;

	prepare_config(&cfg, &fd, file, get_size(file), 0, O_RDWR);
	prepare_map(&map, &cfg);

	/* backup the map and spoil it */
	memcpy(&map_copy, map, sizeof(*map));
	spoil(map, &unmap_required);

	/* unmap the invalid mapping */
	int ret = pmem2_unmap(&map);
	UT_PMEM2_EXPECT_RETURN(ret, exp_ret);

	/* cleanup */
	if (unmap_required)
		unmap_map(&map_copy);

	free(map);
	CLOSE(fd);

	return 1;
}

static void
map_spoil_set_zero_length(struct pmem2_map *map, bool *unmap_rquired)
{
	map->length = 0;
}

static void
map_spoil_set_unaligned_addr(struct pmem2_map *map, bool *unused)
{
	map->addr = (void *)((uintptr_t)map->addr + 1);
	map->length -= 1;
}

static void
map_spoil_by_unmap(struct pmem2_map *map, bool *unmap_required)
{
	unmap_map(map);
	*unmap_required = false;
}

/*
 * test_unmap_zero_length - unmap a pmem2 mapping with an invalid length
 */
static int
test_unmap_zero_length(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s test_unmap_zero_length <file>", argv[0]);

	char *file = argv[0];
	unmap_invalid_common(file, map_spoil_set_zero_length, -EINVAL);

	return 1;
}

/*
 * test_unmap_unaligned_addr - unmap a pmem2 mapping with an unaligned address
 */
static int
test_unmap_unaligned_addr(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s test_unmap_unaligned_addr <file>", argv[0]);

	char *file = argv[0];
	unmap_invalid_common(file, map_spoil_set_unaligned_addr, -EINVAL);

	return 1;
}

/*
 * test_unmap_unaligned_addr - double unmap a pmem2 mapping
 */
static int
test_unmap_unmapped(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s test_unmap_unmapped <file>", argv[0]);

	char *file = argv[0];
	unmap_invalid_common(file, map_spoil_by_unmap, -EINVAL);

	return 1;
}

/*
 * test_map_get_address -- check pmem2_map_get_address func
 */
static int
test_map_get_address(const struct test_case *tc, int argc, char *argv[])
{
	void *ret_addr;
	void *ref_addr = (void *)0x12345;

	struct pmem2_map map;
	map.addr = ref_addr;

	ret_addr = pmem2_map_get_address(&map);
	UT_ASSERTeq(ret_addr, ref_addr);

	return 1;
}

/*
 * test_map_get_size -- check pmem2_map_get_size func
 */
static int
test_map_get_size(const struct test_case *tc, int argc, char *argv[])
{
	size_t ret_size;
	size_t ref_size = 16384;

	struct pmem2_map map;
	map.length = ref_size;

	ret_size = pmem2_map_get_size(&map);
	UT_ASSERTeq(ret_size, ref_size);

	return 1;
}

/*
 * test_get_granularity_simple - simply get the previously stored value
 */
static int
test_get_granularity_simple(const struct test_case *tc, int argc, char *argv[])
{
	struct pmem2_map map;

	map.effective_granularity = PMEM2_GRANULARITY_BYTE;
	enum pmem2_granularity ret = pmem2_map_get_store_granularity(&map);
	UT_ASSERTeq(ret, PMEM2_GRANULARITY_BYTE);

	return 1;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_map_rdrw_file),
	TEST_CASE(test_map_rdonly_file),
	TEST_CASE(test_map_wronly_file),
	TEST_CASE(test_map_valid_ranges),
	TEST_CASE(test_map_invalid_ranges),
	TEST_CASE(test_map_invalid_alignment),
	TEST_CASE(test_map_invalid_fd),
	TEST_CASE(test_map_empty_config),
	TEST_CASE(test_unmap_valid),
	TEST_CASE(test_unmap_zero_length),
	TEST_CASE(test_unmap_unaligned_addr),
	TEST_CASE(test_unmap_unmapped),
	TEST_CASE(test_map_get_address),
	TEST_CASE(test_map_get_size),
	TEST_CASE(test_get_granularity_simple),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmem2_map");
	out_init("pmem2_map", "TEST_LOG_LEVEL", "TEST_LOG_FILE", 0, 0);
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	out_fini();
	DONE(NULL);
}
