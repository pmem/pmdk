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
		fprintf(stderr, "pmem2_config_new: %s\n", pmem2_errormsg());
		exit(1);
	}

	if (pmem2_config_set_fd(cfg, fd)) {
		fprintf(stderr, "pmem2_config_set_fd: %s\n", pmem2_errormsg());
		exit(1);
	}

	if (pmem2_config_set_required_store_granularity(cfg,
			PMEM2_GRANULARITY_PAGE)) {
		fprintf(stderr, "pmem2_config_set_required_store_granularity: "
				"%s\n", pmem2_errormsg());
		exit(1);
	}

	size_t alignment;

	if (pmem2_config_get_alignment(cfg, &alignment)) {
		fprintf(stderr, "pmem2_config_get_alignment: %s\n",
				pmem2_errormsg());
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
		fprintf(stderr, "pmem2_config_set_offset: %s\n",
				pmem2_errormsg());
		exit(1);
	}

	if (pmem2_config_set_length(cfg, length)) {
		fprintf(stderr, "pmem2_config_set_length: %s\n",
				pmem2_errormsg());
		exit(1);
	}

	if (pmem2_map(cfg, &map)) {
		fprintf(stderr, "pmem2_map: %s\n", pmem2_errormsg());
		exit(1);
	}

	char *addr = pmem2_map_get_address(map);
	addr += offset_align;

	for (int i = 0; i < user_length; i++) {
		printf("%02hhX ", addr[i]);
		if (((i + 1) % 16) == 0)
			printf("\n");
	}

	pmem2_unmap(&map);
	pmem2_config_delete(&cfg);
	close(fd);

	return 0;
}
