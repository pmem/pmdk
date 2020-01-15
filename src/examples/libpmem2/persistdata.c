/*
 * Copyright 2020, Intel Corporation
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
 * persistdata.c -- example for using functions associated with persist data
 * in pmem2
 *
 * usage: persistdata src-file offset length
 *
 */

#include <fcntl.h>
#include <libpmem2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CACHELINE (2 << 5)

/*
 * example_ctx -- essential parameters used by example
 */
struct example_ctx {
	int fd;
	enum pmem2_granularity required_granularity;
};

/*
 * header -- XXX
 */
struct header {
	char *head;
	char *tail;
};

/*
 * node -- XXX
 */
struct node {
	char *head;
};

int
main(int argc, char *argv[])
{
	if (argc != 3) {
		fprintf(stderr, "usage: persistdata <file> <granularity>\n");
		exit(1);
	}

	struct example_ctx ctx = {0};

	char *file = argv[1];
	if ((ctx.fd = open(file, O_RDWR)) < 0) {
		perror("creat");
		exit(1);
	}

	ctx.required_granularity = atoi(argv[2]);

	struct pmem2_config *cfg;
	int ret = pmem2_config_new(&cfg);
	if (ret != 0) {
		printf("pmem2_config_new failed: %s\n", pmem2_errormsg());
	}

	ret = pmem2_config_set_fd(cfg, ctx.fd);
	if (ret != 0) {
		printf("pmem2_config_set_fd failed: %s\n", pmem2_errormsg());
	}

	ret = pmem2_config_set_required_store_granularity(
		cfg, ctx.required_granularity);
	if (ret != 0) {
		printf("pmem2_config_set_required_store_granularity failed: %s\n",
			pmem2_errormsg());
	}

	struct pmem2_map *map;
	ret = pmem2_map(cfg, &map);
	if (ret != 0) {
		printf("pmem2_map failed: %s\n", pmem2_errormsg());
	}

	pmem2_flush_fn flush_fn = pmem2_get_flush_fn(map);
	pmem2_drain_fn drian_fn = pmem2_get_drain_fn(map);
	pmem2_persist_fn persist_fn = pmem2_get_persist_fn(map);

	size_t map_size = pmem2_map_get_size(map);
	struct header head = {0};
	head.head = pmem2_map_get_address(map);
	head.tail = head.head + sizeof(head);
	memcpy(head.head, &head, sizeof(head));

	struct node nnode = {0};
	nnode.head = head.tail;
	int remaining_size = map_size - (head.tail - head.head);
	while (remaining_size >= CACHELINE) {
		memcpy(nnode.head, &nnode, sizeof(nnode));
		flush_fn(nnode.head, sizeof(nnode));
		nnode.head += CACHELINE;
		remaining_size -= CACHELINE;
	}

	drian_fn();

	persist_fn(head.head, pmem2_map_get_size(map));

	return 0;
}
