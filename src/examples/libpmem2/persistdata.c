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

#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <libpmem2.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define ALIGN_UP(size, align) (((size) + (align)-1) & ~((align)-1))
#define CACHELINE 64
#define MAX_WRITES 1000000
#define MAX_WRITE_SIZE 12
#define MAGIC_NUM 7

static const char *source_pattern = "Legia Warszawa to najlepszy klub.";

/*
 * example_ctx -- essential parameters used by example
 */
struct example_ctx {
	int fd;
	struct pmem2_config *cfg;
	struct pmem2_map *map;
	pmem2_flush_fn flush_fn;
	pmem2_drain_fn drian_fn;
	pmem2_persist_fn persist_fn;
	char mode;

	/* user arguments */
	char *path;
	size_t n_entries;
};

/*
 * aof_header_t -- AOF header
 */
struct aof_header_t {
	size_t offset;
	size_t elements_number;
	void *tail;
};

/*
 * aof_elem_t -- AOF element
 */
struct aof_elem_t {
	size_t id;
	size_t writes_size;
	size_t checksum;
	char pattern[];
};

/*
 * print_usage -- print short description of usage
 */
static void
print_usage(void)
{
	printf("Usage:	append_data -h\n"
		"	append_data <-a|-p> <file>\n"
		"Available options:\n"
		"-a, --append=N	- add N elements to the <file>, default value N=1024\n"
		"-p, --print	- print all elements from the <file>\n"
		"-v, --validate	- validate all elements from the <file>\n"
		"-h, --help	- print this usage info\n");
}

/*
 * long_options -- command line options
 */
static const struct option long_options[] = {
	{"append", required_argument, NULL, 'a'},
	{"print", no_argument, NULL, 'p'},
	{"validate", no_argument, NULL, 'v'},
	{"help", no_argument, NULL, 'h'},
	{NULL, 0, NULL, 0},
};

/*
 * parse_args -- parse command line arguments
 */
static int
parse_args(int argc, char *argv[], struct example_ctx *ctx)
{
	ctx->mode = '0';
	int opt;
	while ((opt = getopt_long(argc, argv, "aphv", long_options, NULL)) !=
		-1) {
		switch (opt) {
			case 'a':
				ctx->mode = 'a';
				if (!optarg)
					ctx->n_entries = 1024;
				else
					ctx->n_entries = atoi(optarg);
				break;
			case 'p':
				ctx->mode = 'p';
				break;
			case 'v':
				ctx->mode = 'v';
				break;
			case 'h':
				print_usage();
				exit(0);
			default:
				print_usage();
				return 1;
		}
	}
	if (optind < argc) {
		ctx->path = argv[optind];
	} else {
		fprintf(stderr, "append_data: file cannot be empty.\n");
		print_usage();
		return 1;
	}
	if (ctx->mode == '0') {
		fprintf(stderr,
			"append_data: number of elements N is not defined.\n");
		print_usage();
		return 1;
	}

	return 0;
}

static void
example_init(struct example_ctx *ctx)
{
	if ((ctx->fd = open(ctx->path, O_RDWR)) < 0) {
		perror("creat");
		exit(1);
	}

	int ret = pmem2_config_new(&ctx->cfg);
	if (ret != 0) {
		printf("pmem2_config_new failed: %s\n", pmem2_errormsg());
		exit(ret);
	}

	ret = pmem2_config_set_fd(ctx->cfg, ctx->fd);
	if (ret != 0) {
		printf("pmem2_config_set_fd failed: %s\n", pmem2_errormsg());
		exit(ret);
	}

	ret = pmem2_config_set_required_store_granularity(
		ctx->cfg, PMEM2_GRANULARITY_PAGE);
	if (ret != 0) {
		printf("pmem2_config_set_required_store_granularity failed: "
			"%s\n", pmem2_errormsg());
		exit(ret);
	}

	ret = pmem2_map(ctx->cfg, &ctx->map);
	if (ret != 0) {
		printf("pmem2_map failed: %s\n", pmem2_errormsg());
		exit(ret);
	}

	ctx->flush_fn = pmem2_get_flush_fn(ctx->map);
	ctx->drian_fn = pmem2_get_drain_fn(ctx->map);
	ctx->persist_fn = pmem2_get_persist_fn(ctx->map);
}

static struct aof_header_t *
init_header(struct example_ctx *ctx)
{
	struct aof_header_t *header = pmem2_map_get_address(ctx->map);
	if (!header->offset) {
		header->offset = sizeof(*header);
		header->elements_number = 0;
		header->tail = (void *)((uintptr_t)header + sizeof(*header));
	} else
		header->tail = (void *)((uintptr_t)header + header->offset);

	return header;
}

static struct aof_elem_t *
init_elem(void *addr, int id)
{
	struct aof_elem_t *elem = addr;
	elem->id = id;

	return elem;
}

static size_t
calc_checksum(struct aof_elem_t *elem)
{
	size_t checksum = ((size_t)elem->id + (size_t)elem->pattern[0] +
			(size_t)elem->writes_size) % MAGIC_NUM;

	return checksum;
}

static void
update_header(struct aof_header_t *header, size_t off, void *addr,
		size_t n_elem)
{
	header->offset = off;
	header->tail = addr;
	header->elements_number = n_elem;
}

static int
fill_element(struct aof_elem_t *elem, size_t used_memory, size_t map_size)
{
	elem->writes_size =
		ALIGN_UP(rand() % MAX_WRITE_SIZE + sizeof(*elem), CACHELINE);
	if ((used_memory + elem->writes_size) > map_size)
		return 1;
	memset(elem->pattern, source_pattern[elem->id % 34],
		elem->writes_size - sizeof(*elem) - 1);
	elem->pattern[elem->writes_size - sizeof(*elem)] = '\0';
	elem->checksum = calc_checksum(elem);

	return 0;
}

static void
append_n_elements(struct aof_header_t *header, struct example_ctx ctx)
{
	struct aof_elem_t *elem =
		init_elem(header->tail, header->elements_number);

	size_t used_memory = header->offset;
	/* Flushes number, after which will run drain_fn() */
	int n_flushes = rand() % ctx.n_entries;

	size_t map_size = pmem2_map_get_size(ctx.map);

	for (int i = 0; i < ctx.n_entries; ++i) {
		if (n_flushes == 0) {
			ctx.drian_fn();
			n_flushes = rand() % ctx.n_entries;
			update_header(header, used_memory, elem, elem->id);
			ctx.persist_fn(header, sizeof(*header));
		}

		if (fill_element(elem, used_memory, map_size))
			break;
		ctx.flush_fn(elem, elem->writes_size);
		--n_flushes;
		used_memory += elem->writes_size;

		/* Move to the next element */
		void *addr = (void *)((uintptr_t)elem +
					(uintptr_t)elem->writes_size);
		struct aof_elem_t *next_elem = init_elem(addr, elem->id + 1);
		elem = next_elem;
	}
	ctx.drian_fn();
	update_header(header, used_memory, elem, elem->id);
	ctx.persist_fn(header, sizeof(*header));
	printf("%li\n", used_memory);
}

static void
print_all_elements(struct aof_header_t *header, struct example_ctx ctx)
{
	FILE *log = fopen("pattern.log", "a");
	struct aof_elem_t *curr_elem =
		(struct aof_elem_t *)((uintptr_t)header + sizeof(*header));
	while ((void *)curr_elem < (void *)header->tail) {
		fprintf(log, "%s", curr_elem->pattern);
		curr_elem = (struct aof_elem_t *)((uintptr_t)curr_elem +
							curr_elem->writes_size);
	}
}

static void
validate_file(struct aof_header_t *header, struct example_ctx ctx)
{
	struct aof_elem_t *curr_elem =
		(struct aof_elem_t *)((uintptr_t)header + sizeof(*header));
	size_t i = 0;
	while ((void *)curr_elem < (void *)header->tail) {
		size_t checksum = calc_checksum(curr_elem);
		assert(checksum == curr_elem->checksum);
		curr_elem = (struct aof_elem_t *)((uintptr_t)curr_elem +
							curr_elem->writes_size);
		++i;
	}
	assert(i == header->elements_number);
}

int
main(int argc, char *argv[])
{
	struct example_ctx ctx = {0};

	int ret = 0;
	if (parse_args(argc, argv, &ctx)) {
		ret = 1;
		goto out;
	}

	example_init(&ctx);
	/* Init and persist header */
	struct aof_header_t *header = init_header(&ctx);
	ctx.persist_fn(header, sizeof(*header));

	switch (ctx.mode) {
		case 'a':
			append_n_elements(header, ctx);
			break;

		case 'p':
			print_all_elements(header, ctx);
			break;
		case 'v':
			validate_file(header, ctx);
			break;

		default:
			return 1;
	}

out:
	return ret;
}
