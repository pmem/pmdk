// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

/*
 * advanced.c -- example for the libpmem2
 *
 *  * usage: advanced src-file offset length
 *
 */

#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libpmem2.h>

#ifndef _WIN32
#include <unistd.h>
#else
#include <io.h>
#endif

int
main(int argc, char *argv[])
{
	int fd;
	struct pmem2_config *cfg;
	struct pmem2_map *map;
	struct pmem2_source *src;

	if (argc != 4) {
		fprintf(stderr, "usage: %s src-file offset length\n", argv[0]);
		exit(1);
	}

	size_t offset = atoi(argv[2]);
	size_t user_length = atoi(argv[3]);
	size_t length = user_length;

	if ((fd = open(argv[1], O_RDWR)) < 0) {
		perror("open");
		exit(1);
	}

	if (pmem2_config_new(&cfg)) {
		pmem2_perror("pmem2_config_new");
		exit(1);
	}

	if (pmem2_source_from_fd(&src, fd)) {
		pmem2_perror("pmem2_source_from_fd");
		exit(1);
	}

	if (pmem2_config_set_required_store_granularity(cfg,
			PMEM2_GRANULARITY_PAGE)) {
		pmem2_perror("pmem2_config_set_required_store_granularity");
		exit(1);
	}

	size_t alignment;

	if (pmem2_source_alignment(src, &alignment)) {
		pmem2_perror("pmem2_source_alignment");
		exit(1);
	}

	size_t offset_align = offset % alignment;

	if (offset_align != 0) {
		offset = offset - offset_align;
		length += offset_align;
	}

	size_t len_align = length % alignment;

	if (len_align != 0)
		length += (alignment - len_align);

	if (pmem2_config_set_offset(cfg, offset)) {
		pmem2_perror("pmem2_config_set_offset");
		exit(1);
	}

	if (pmem2_config_set_length(cfg, length)) {
		pmem2_perror("pmem2_config_set_length");
		exit(1);
	}

	if (pmem2_map(&map, cfg, src)) {
		pmem2_perror("pmem2_map");
		exit(1);
	}

	char *addr = pmem2_map_get_address(map);
	addr += offset_align;

	for (size_t i = 0; i < user_length; i++) {
		printf("%02hhX ", addr[i]);
		if ((i & 0x0F) == 0x0F)
			printf("\n");
	}

	pmem2_unmap(&map);
	pmem2_source_delete(&src);
	pmem2_config_delete(&cfg);
	close(fd);

	return 0;
}
