// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

/*
 * basic.c -- simple example for the libpmemset
 */

#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#else
#include <io.h>
#endif
#include <libpmem2.h>
#include <libpmemset.h>

int
main(int argc, char *argv[])
{
	int fd;
	struct pmem2_source *pmem2_src;
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_source *src;
	struct pmemset_part *part;
	struct pmemset_part_descriptor desc;

	if (argc != 2) {
		fprintf(stderr, "usage: %s file\n", argv[0]);
		exit(1);
	}

	if ((fd = open(argv[1], O_RDWR)) < 0) {
		perror("open");
		exit(1);
	}

	if (pmem2_source_from_fd(&pmem2_src, fd)) {
		pmem2_perror("pmem2_source_from_fd");
		exit(1);
	}

	if (pmemset_source_from_pmem2(&src, pmem2_src)) {
		pmemset_perror("pmemset_source_from_pmem2");
		exit(1);
	}

	if (pmemset_config_new(&cfg)) {
		pmemset_perror("pmemset_config_new");
		exit(1);
	}

	if (pmemset_new(&set, cfg)) {
		pmemset_perror("pmemset_new");
		exit(1);
	}

	if (pmemset_part_new(&part, set, src, 0, 0)) {
		pmemset_perror("pmemset_part_new");
		exit(1);
	}

	if (pmemset_part_map(&part, NULL, &desc)) {
		pmemset_perror("pmemset_part_new");
		exit(1);
	}

	strcpy(desc.addr, "hello, persistent memory");

	pmemset_persist(set, desc.addr, desc.size);  /* XXX: doesn't work */

	pmem2_source_delete(&pmem2_src);

	pmemset_source_delete(&src);
	pmemset_config_delete(&cfg);
	pmemset_delete(&set);
	close(fd);

	return 0;
}
