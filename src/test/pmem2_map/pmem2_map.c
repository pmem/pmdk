// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

/*
 * pmem2_map.c -- pmem2_map unittests
 */

#include <stdbool.h>

#include "config.h"
#include "pmem2_utils.h"
#include "source.h"
#include "map.h"
#include "out.h"
#include "pmem2.h"
#include "unittest.h"
#include "ut_pmem2.h"
#include "ut_pmem2_setup.h"

#define KILOBYTE (1 << 10)
#define MEGABYTE (1 << 20)

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
prepare_map(struct pmem2_map **map_ptr,
	struct pmem2_config *cfg, struct pmem2_source *src)
{
	struct pmem2_map *map = malloc(sizeof(*map));
	UT_ASSERTne(map, NULL);

	UT_ASSERTeq(src->type, PMEM2_SOURCE_HANDLE);

	size_t max_size = cfg->length + cfg->offset;
	HANDLE mh = CreateFileMapping(src->value.handle,
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

	map->reserved_length = map->content_length = cfg->length;
	map->effective_granularity = PMEM2_GRANULARITY_PAGE;
	map->reserv = NULL;

	*map_ptr = map;

	UT_ASSERTeq(pmem2_register_mapping(map), 0);
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
prepare_map(struct pmem2_map **map_ptr,
	struct pmem2_config *cfg, struct pmem2_source *src)
{
	int flags = MAP_SHARED;
	int proto = PROT_READ | PROT_WRITE;

	off_t offset = (off_t)cfg->offset;
	UT_ASSERTeq((size_t)offset, cfg->offset);

	struct pmem2_map *map = malloc(sizeof(*map));
	UT_ASSERTne(map, NULL);

	UT_ASSERTeq(src->type, PMEM2_SOURCE_FD);
	map->addr = mmap(NULL, cfg->length, proto, flags,
		src->value.fd, offset);
	UT_ASSERTne(map->addr, MAP_FAILED);

	map->source.value.ftype = PMEM2_FTYPE_REG;
	map->reserved_length = map->content_length = cfg->length;
	map->effective_granularity = PMEM2_GRANULARITY_PAGE;
	map->reserv = NULL;

	*map_ptr = map;

	UT_ASSERTeq(pmem2_register_mapping(map), 0);
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
	UT_ASSERTeq(munmap(map->addr, map->reserved_length), 0);
#endif
	UT_ASSERTeq(pmem2_unregister_mapping(map), 0);
}

/*
 * get_align_by_name -- fetch map alignment for an unopened file
 */
static size_t
get_align_by_name(const char *filename)
{
	struct pmem2_source *src;
	size_t align;
	int fd = OPEN(filename, O_RDONLY);
	PMEM2_SOURCE_FROM_FD(&src, fd);
	PMEM2_SOURCE_ALIGNMENT(src, &align);
	PMEM2_SOURCE_DELETE(&src);
	CLOSE(fd);

	return align;
}

/*
 * test_map_rdrw_file - map a O_RDWR file
 */
static int
test_map_rdrw_file(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_map_rdrw_file <file>");

	char *file = argv[0];
	struct pmem2_config cfg;
	struct pmem2_source *src;
	struct FHandle *fh;
	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, 0, 0, FH_RDWR);

	struct pmem2_map *map;
	int ret = pmem2_map_new(&map, &cfg, src);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	unmap_map(map);
	FREE(map);
	PMEM2_SOURCE_DELETE(&src);
	UT_FH_CLOSE(fh);

	return 1;
}

/*
 * test_map_rdonly_file - map a O_RDONLY file
 */
static int
test_map_rdonly_file(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_map_rdonly_file <file>");

	char *file = argv[0];
	struct pmem2_config cfg;
	struct pmem2_source *src;
	struct FHandle *fh;
	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, 0, 0, FH_READ);

	struct pmem2_map *map;
	int ret = pmem2_map_new(&map, &cfg, src);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_NO_ACCESS);

	PMEM2_SOURCE_DELETE(&src);
	UT_FH_CLOSE(fh);

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
	struct pmem2_source *src;
	struct pmem2_map *map;
	int ret = 0;
	struct FHandle *fh;

	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, length, offset,
				FH_RDWR);
	ret = pmem2_map_new(&map, &cfg, src);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(map->content_length, val_length);

	unmap_map(map);
	FREE(map);
	PMEM2_SOURCE_DELETE(&src);
	UT_FH_CLOSE(fh);
}

/*
 * test_map_valid_ranges - map valid memory ranges
 */
static int
test_map_valid_ranges(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_map_valid_ranges <file> <size>");

	char *file = argv[0];
	size_t align = get_align_by_name(file);
	size_t size = ATOUL(argv[1]);
	size_t size2 = ALIGN_DOWN(size / 2, align);

	/* the config WITHOUT provided length allows mapping the whole file */
	map_valid_ranges_common(file, 0, 0, size);

	/* the config WITH provided length allows mapping the whole file */
	map_valid_ranges_common(file, 0, size, size);

	/* the config with provided length different than the file length */
	map_valid_ranges_common(file, 0, size2, size2);

	/* verify the config with provided length and a valid offset */
	map_valid_ranges_common(file, align, size2, size2);

	return 2;
}

/*
 * test_map_invalid_ranges - map invalid memory ranges
 */
static int
test_map_invalid_ranges(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_map_invalid_ranges <file> <size>");

	char *file = argv[0];
	struct pmem2_config cfg;
	struct pmem2_source *src;
	size_t size = ATOUL(argv[1]);
	size_t offset = 0;
	struct pmem2_map *map;
	int ret = 0;
	struct FHandle *fh;

	/* the mapping + the offset > the file size */
	size_t size2 = ALIGN_DOWN(size / 2, get_align_by_name(file));
#if defined(__PPC64__)
	offset = size2 + (64 * MEGABYTE);
#else
	offset = size2 + (4 * MEGABYTE);
#endif
	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, size2, offset,
				FH_RDWR);
	ret = pmem2_map_new(&map, &cfg, src);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_MAP_RANGE);
	PMEM2_SOURCE_DELETE(&src);
	UT_FH_CLOSE(fh);

	/* the mapping size > the file size */
	offset = size * 2;
	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, 0, offset,
				FH_RDWR);
	ret = pmem2_map_new(&map, &cfg, src);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_MAP_RANGE);
	PMEM2_SOURCE_DELETE(&src);
	UT_FH_CLOSE(fh);

	return 2;
}

/*
 * test_map_invalid_alignment - map using invalid alignment in the offset
 */
static int
test_map_invalid_alignment(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_map_invalid_args <file> <size>");

	char *file = argv[0];
	struct pmem2_config cfg;
	struct pmem2_source *src;
	size_t size = ATOUL(argv[1]);
	size_t length =  size / 2;
	struct pmem2_map *map;
	struct FHandle *fh;

	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, length,
				KILOBYTE, FH_RDWR);
	int ret = pmem2_map_new(&map, &cfg, src);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_OFFSET_UNALIGNED);
	PMEM2_SOURCE_DELETE(&src);
	UT_FH_CLOSE(fh);

	return 2;
}

/*
 * test_map_invalid_fd - map using a invalid file descriptor
 */
static int
test_map_invalid_fd(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_map_invalid_args <file> <size>");

	char *file = argv[0];
	struct pmem2_config cfg;
	struct pmem2_source *src;
	size_t size = ATOUL(argv[1]);
	size_t length = size / 2;
	struct pmem2_map *map;
	struct FHandle *fh;

	/* the invalid file descriptor */
	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, length, 0,
				FH_RDWR);
	UT_FH_CLOSE(fh);
	int ret = pmem2_map_new(&map, &cfg, src);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_INVALID_FILE_HANDLE);

	PMEM2_SOURCE_DELETE(&src);

	return 2;
}

/*
 * test_map_unaligned_length - map a file of length which is not page-aligned
 */
static int
test_map_unaligned_length(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_map_unaligned_length <file> <size>");

	char *file = argv[0];
	struct pmem2_config cfg;
	struct pmem2_source *src;
	size_t length = ATOUL(argv[1]);
	struct pmem2_map *map;
	struct FHandle *fh;

	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, length, 0,
				FH_RDWR);
	int ret = pmem2_map_new(&map, &cfg, src);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_LENGTH_UNALIGNED);
	PMEM2_SOURCE_DELETE(&src);
	UT_FH_CLOSE(fh);

	return 2;
}

/*
 * test_unmap_valid - unmap valid pmem2 mapping
 */
static int
test_unmap_valid(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_unmap_valid <file> <size>");

	char *file = argv[0];
	size_t size = ATOUL(argv[1]);
	struct pmem2_config cfg;
	struct pmem2_source *src;
	struct pmem2_map *map = NULL;
	struct FHandle *fh;

	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, size, 0,
				FH_RDWR);
	prepare_map(&map, &cfg, src);

	/* unmap the valid mapping */
	int ret = pmem2_map_delete(&map);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(map, NULL);
	PMEM2_SOURCE_DELETE(&src);
	UT_FH_CLOSE(fh);

	return 2;
}

typedef void (*spoil_func)(struct pmem2_map *map);

/*
 * unmap_invalid_common - unmap an invalid pmem2 mapping
 */
static int
unmap_invalid_common(const char *file, size_t size,
	spoil_func spoil, int exp_ret)
{
	struct pmem2_config cfg;
	struct pmem2_source *src;
	struct pmem2_map *map = NULL;
	struct pmem2_map map_copy;
	struct FHandle *fh;

	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, size, 0,
				FH_RDWR);
	prepare_map(&map, &cfg, src);

	/* backup the map and spoil it */
	memcpy(&map_copy, map, sizeof(*map));
	spoil(map);

	/* unmap the invalid mapping */
	int ret = pmem2_map_delete(&map);
	UT_PMEM2_EXPECT_RETURN(ret, exp_ret);

	FREE(map);
	PMEM2_SOURCE_DELETE(&src);
	UT_FH_CLOSE(fh);

	return 1;
}

static void
map_spoil_set_zero_length(struct pmem2_map *map)
{
	map->reserved_length = 0;
	map->content_length = 0;
}

static void
map_spoil_set_unaligned_addr(struct pmem2_map *map)
{
	map->addr = (void *)((uintptr_t)map->addr + 1);
	map->reserved_length -= 1;
}

static void
map_spoil_by_unmap(struct pmem2_map *map)
{
	unmap_map(map);
}

/*
 * test_unmap_zero_length - unmap a pmem2 mapping with an invalid length
 */
static int
test_unmap_zero_length(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_unmap_zero_length <file> <size>");

	char *file = argv[0];
	size_t size = ATOUL(argv[1]);
	unmap_invalid_common(file, size, map_spoil_set_zero_length,
			PMEM2_E_MAPPING_NOT_FOUND);

	return 2;
}

/*
 * test_unmap_unaligned_addr - unmap a pmem2 mapping with an unaligned address
 */
static int
test_unmap_unaligned_addr(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_unmap_unaligned_addr <file> <size>");

	char *file = argv[0];
	size_t size = ATOUL(argv[1]);
	unmap_invalid_common(file, size, map_spoil_set_unaligned_addr, -EINVAL);

	return 2;
}

/*
 * test_unmap_unaligned_addr - double unmap a pmem2 mapping
 */
static int
test_unmap_unmapped(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_unmap_unmapped <file> <size>");

	char *file = argv[0];
	size_t size = ATOUL(argv[1]);
	unmap_invalid_common(file, size, map_spoil_by_unmap,
			PMEM2_E_MAPPING_NOT_FOUND);

	return 2;
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

	return 0;
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
	map.content_length = ref_size;

	ret_size = pmem2_map_get_size(&map);
	UT_ASSERTeq(ret_size, ref_size);

	return 0;
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

	return 0;
}

/*
 * test_map_larger_than_unaligned_file_size - map a file which size is not
 * aligned
 */
static int
test_map_larger_than_unaligned_file_size(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_map_larger_than_unaligned_file_size"
			" <file> <size>");

	char *file = argv[0];
	struct pmem2_config cfg;
	struct pmem2_source *src;
	size_t length = ATOUL(argv[1]);
	struct pmem2_map *map;
	struct FHandle *fh;
	size_t alignment;
	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, 0, 0, FH_RDWR);

	PMEM2_SOURCE_ALIGNMENT(src, &alignment);

	/* validate file length is unaligned */
	UT_ASSERTne(length % alignment, 0);

	/* align up the required mapping length */
	cfg.length = ALIGN_UP(length, alignment);

	int ret = pmem2_map_new(&map, &cfg, src);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	unmap_map(map);
	FREE(map);
	PMEM2_SOURCE_DELETE(&src);
	UT_FH_CLOSE(fh);

	return 2;
}

/*
 * test_map_zero_file_size - map using zero file size, do not set length
 * in config, expect failure
 */
static int
test_map_zero_file_size(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_map_zero_file_size <file>");

	char *file = argv[0];
	int fd = OPEN(file, O_RDWR);
	if (fd < 0)
		UT_FATAL("open: %s", file);

	struct pmem2_config cfg;
	pmem2_config_init(&cfg);

	/* mapping length is left unset */
	cfg.offset = 0;
	cfg.requested_max_granularity = PMEM2_GRANULARITY_PAGE;

	struct pmem2_source *src;
	PMEM2_SOURCE_FROM_FD(&src, fd);

	struct pmem2_map *map;
	int ret = pmem2_map_new(&map, &cfg, src);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_SOURCE_EMPTY);

	PMEM2_SOURCE_DELETE(&src);
	CLOSE(fd);

	return 2;
}

static void
do_map_and_copy_data(struct pmem2_config *cfg, struct pmem2_source *src,
			struct pmem2_map **map, const char *data)
{
	int ret = pmem2_map_new(map, cfg, src);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	pmem2_memcpy_fn memcpy_fn = pmem2_get_memcpy_fn(*map);
	void *addr = pmem2_map_get_address(*map);
	memcpy_fn(addr, data, strlen(data), 0);
	UT_ASSERTeq(memcmp(addr, data, strlen(data)), 0);
}

static const char *word1 = "Persistent or nonpersistent: that is the question.";
static const char *word2 = "Nonpersistent: that is the answer.";

/*
 * test_map_sharing_shared - map file with the PMEM2_SHARED option and check if
 * data was written; the file is not reopened
 */
static int
test_map_sharing_shared(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_map_sharing_shared <file>");

	char *file = argv[0];

	struct pmem2_config cfg;
	struct pmem2_source *src;
	struct FHandle *fh;

	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, 0, 0, FH_RDWR);

	struct pmem2_map *map1 = NULL;
	do_map_and_copy_data(&cfg, src, &map1, word1);

	struct pmem2_map *map2 = NULL;
	do_map_and_copy_data(&cfg, src, &map2, word2);

	void *addr1 = pmem2_map_get_address(map1);
	/* check if changes in shared mapping affect other mapping */
	UT_ASSERTeq(memcmp(addr1, word2, strlen(word2)), 0);
	UT_ASSERTne(memcmp(addr1, word1, strlen(word1)), 0);

	unmap_map(map2);
	unmap_map(map1);
	FREE(map2);
	FREE(map1);
	PMEM2_SOURCE_DELETE(&src);
	UT_FH_CLOSE(fh);

	return 1;
}

/*
 * test_map_sharing_private - map file with the PMEM2_PRIVATE option and
 * check if data wasn't written; the file is not reopen
 */
static int
test_map_sharing_private(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_map_sharing_private <file>");

	char *file = argv[0];

	struct pmem2_config cfg;
	struct pmem2_source *src;
	struct FHandle *fh;

	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, 0, 0, FH_RDWR);

	struct pmem2_map *map1 = NULL;
	do_map_and_copy_data(&cfg, src, &map1, word1);

	struct pmem2_map *map2 = NULL;
	pmem2_config_set_sharing(&cfg, PMEM2_PRIVATE);
	do_map_and_copy_data(&cfg, src, &map2, word2);

	void *addr1 = pmem2_map_get_address(map1);
	/* check if changes in private mapping do not affect other mapping */
	UT_ASSERTne(memcmp(addr1, word2, strlen(word2)), 0);
	UT_ASSERTeq(memcmp(addr1, word1, strlen(word1)), 0);

	unmap_map(map2);
	unmap_map(map1);
	FREE(map2);
	FREE(map1);
	PMEM2_SOURCE_DELETE(&src);
	UT_FH_CLOSE(fh);

	return 1;
}

/*
 * test_map_sharing_private_with_reopened_fd - map file, with the PMEM2_PRIVATE
 * option and check if data wasn't written; the file is reopened before every
 * mapping
 */
static int
test_map_sharing_private_with_reopened_fd(const struct test_case *tc, int argc,
					char *argv[])
{
	if (argc < 1)
		UT_FATAL(
		"usage: test_map_sharing_private_with_reopened_fd <file>");

	char *file = argv[0];

	struct pmem2_config cfg;
	struct pmem2_source *src;
	struct FHandle *fh1;

	ut_pmem2_prepare_config(&cfg, &src, &fh1, FH_FD, file, 0, 0,
				FH_RDWR);

	struct pmem2_map *map1;
	do_map_and_copy_data(&cfg, src, &map1, word1);
	UT_FH_CLOSE(fh1);

	struct FHandle *fh2;
	ut_pmem2_prepare_config(&cfg, &src, &fh2, FH_FD, file, 0, 0,
				FH_RDWR);
	struct pmem2_map *map2;
	pmem2_config_set_sharing(&cfg, PMEM2_PRIVATE);
	do_map_and_copy_data(&cfg, src, &map2, word2);
	UT_FH_CLOSE(fh2);

	void *addr1 = pmem2_map_get_address(map1);
	/* check if changes in private mapping do not affect other mapping */
	UT_ASSERTne(memcmp(addr1, word2, strlen(word2)), 0);
	UT_ASSERTeq(memcmp(addr1, word1, strlen(word1)), 0);

	unmap_map(map2);
	unmap_map(map1);
	FREE(map2);
	FREE(map1);
	PMEM2_SOURCE_DELETE(&src);

	return 1;
}

/*
 * test_map_sharing_private_rdonly_file - map O_RDONLY file with
 * PMEM2_PRIVATE sharing
 */
static int
test_map_sharing_private_rdonly_file(const struct test_case *tc, int argc,
					char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_map_sharing_private_rdonly_file <file>");

	char *file = argv[0];

	struct pmem2_config cfg;
	struct pmem2_source *src;
	struct FHandle *fh;

	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, 0, 0, FH_READ);
	pmem2_config_set_sharing(&cfg, PMEM2_PRIVATE);

	struct pmem2_map *map = NULL;
	do_map_and_copy_data(&cfg, src, &map, word2);

	unmap_map(map);
	FREE(map);
	PMEM2_SOURCE_DELETE(&src);
	UT_FH_CLOSE(fh);

	return 1;
}

/*
 * test_map_sharing_private_devdax - map DAX device with PMEM2_PRIVATE sharing
 */
static int
test_map_sharing_private_devdax(const struct test_case *tc, int argc,
					char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_map_sharing_private_devdax <file>");

	char *file = argv[0];

	struct pmem2_config cfg;
	struct pmem2_source *src;
	struct FHandle *fh;

	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, 0, 0, FH_RDWR);
	pmem2_config_set_sharing(&cfg, PMEM2_PRIVATE);

	struct pmem2_map *map = NULL;
	int ret = pmem2_map_new(&map, &cfg, src);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_SRC_DEVDAX_PRIVATE);
	UT_ASSERTeq(map, NULL);

	PMEM2_SOURCE_DELETE(&src);
	UT_FH_CLOSE(fh);

	return 1;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_map_rdrw_file),
	TEST_CASE(test_map_rdonly_file),
	TEST_CASE(test_map_valid_ranges),
	TEST_CASE(test_map_invalid_ranges),
	TEST_CASE(test_map_invalid_alignment),
	TEST_CASE(test_map_invalid_fd),
	TEST_CASE(test_map_unaligned_length),
	TEST_CASE(test_unmap_valid),
	TEST_CASE(test_unmap_zero_length),
	TEST_CASE(test_unmap_unaligned_addr),
	TEST_CASE(test_unmap_unmapped),
	TEST_CASE(test_map_get_address),
	TEST_CASE(test_map_get_size),
	TEST_CASE(test_get_granularity_simple),
	TEST_CASE(test_map_larger_than_unaligned_file_size),
	TEST_CASE(test_map_zero_file_size),
	TEST_CASE(test_map_sharing_shared),
	TEST_CASE(test_map_sharing_private),
	TEST_CASE(test_map_sharing_private_with_reopened_fd),
	TEST_CASE(test_map_sharing_private_rdonly_file),
	TEST_CASE(test_map_sharing_private_devdax),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmem2_map");
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	DONE(NULL);
}

#ifdef _MSC_VER
MSVC_CONSTR(libpmem2_init)
MSVC_DESTR(libpmem2_fini)
#endif
