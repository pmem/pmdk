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
 * aof.c -- Naive but "persistently-correct" implementation of the append-only
 * file structure. The user is doing multiple appends and flushes them all.
 * When all is done (or at some picked checkpoint), program drain all previous
 * flushes, updates the header (especially information about the end of
 * the valid data in the file) and persist the changes done to the header.
 */

#include <assert.h>
#include <ex_common.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/types.h>

#include <libpmem2.h>

#if defined(__x86_64) || defined(_M_X64) || defined(__aarch64__)
#define CACHELINE_SIZE 64ULL
#elif defined(__PPC64__)
#define CACHELINE_SIZE 128ULL
#else
#error unable to recognize architecture at compile time
#endif

#define ALIGN_UP(size, align) (((size) + (align)-1) & ~((align)-1))
#define MAX_WRITE_SIZE 1024
#define MAGIC_NUM 727

#define MODE_APPEND 'a'
#define MODE_PRINT 'p'
#define MODE_VALIDATE 'v'

static const char *src_pattern = "Example sentence.";
#define SRC_PATTERN_LEN (strlen(src_pattern))

/*
 * aof -- append-only file application context
 */
struct aof {
	int fd;
	struct pmem2_map *map;

	pmem2_flush_fn flush_fn;
	pmem2_drain_fn drian_fn;
	pmem2_persist_fn persist_fn;

	/* user arguments */
	char *path;
	size_t flush_stride;
	char mode;
	size_t n_new_entries;
};

/*
 * aof_header -- AOF header
 */
struct aof_header {
	size_t offset;
	size_t n_elems;

	/* volatile */
	void *tail;
};

/*
 * aof_elem -- AOF element
 */
struct aof_elem {
	size_t id;
	size_t elem_size;
	size_t checksum;
	char pattern[];
};

/*
 * print_usage -- print short description of usage
 */
static void
print_usage(void)
{
	printf("Usage:\taof -h\n"
		"\taof <-a|-p N> [-s X] <file>\n"
		"Available options:\n"
		"-a N,\t--append=N\t- add N elements to the <file>\n"
		"-p,\t--print\t\t- print all elements from the <file>\n"
		"-s X,\t--stride=X\t- set stride X according to which program persist data\n"
		"-v,\t--validate\t- validate all elements from the <file>\n"
		"-h,\t--help\t\t- print this usage info\n");
}

/*
 * long_options -- command line options
 */
static const struct option long_options[] = {
	{"append",	required_argument,	NULL, MODE_APPEND},
	{"print",	no_argument,		NULL, MODE_PRINT},
	{"stride",	required_argument,	NULL, 's'},
	{"validate",	no_argument,		NULL, MODE_VALIDATE},
	{"help",	no_argument,		NULL, 'h'},
	{NULL,		0,			NULL, 0},
};

/*
 * parse_args -- parse command line arguments
 */
static void
parse_args(int argc, char *argv[], struct aof *ctx)
{
	ctx->mode = '0';
	ctx->flush_stride = 7;
	int opt;
	while ((opt = getopt_long(argc, argv, "a:ps:vh", long_options, NULL)) !=
		-1) {
		switch (opt) {
			case MODE_APPEND:
				ctx->mode = MODE_APPEND;
				ctx->n_new_entries = strtoul(optarg, NULL, 0);
				if (!ctx->n_new_entries) {
					fprintf(stderr, "aof: wrong N value\n");
					print_usage();
					exit(1);
				}
				break;
			case MODE_PRINT:
				ctx->mode = MODE_PRINT;
				break;
			case MODE_VALIDATE:
				ctx->mode = MODE_VALIDATE;
				break;
			case 's':
				ctx->flush_stride = strtoul(optarg, NULL, 0);
				if (!ctx->flush_stride) {
					fprintf(stderr, "aof: wrong X value\n");
					print_usage();
					exit(1);
				}
				break;
			case 'h':
				print_usage();
				exit(0);
			default:
				print_usage();
				exit(1);
		}
	}
	if (optind < argc) {
		ctx->path = argv[optind];
	} else {
		fprintf(stderr, "aof: missing required argument: <file>\n");
		print_usage();
		exit(1);
	}
	if (ctx->mode == '0') {
		fprintf(stderr,
			"aof: action not specified, either: append, print, validate\n");
		print_usage();
		exit(1);
	}
}

/*
 * aof_init -- prepare AOF structure to use
 */
static void
aof_init(struct aof *ctx)
{
	if ((ctx->fd = open(ctx->path, O_RDWR)) < 0) {
		perror("open");
		exit(1);
	}

	struct pmem2_config *cfg;
	int ret = pmem2_config_new(&cfg);
	if (ret) {
		fprintf(stderr, "pmem2_config_new: %s\n", pmem2_errormsg());
		goto err_config_new;
	}

	ret = pmem2_config_set_fd(cfg, ctx->fd);
	if (ret) {
		fprintf(stderr, "pmem2_config_set_fd: %s\n", pmem2_errormsg());
		goto err_config_set;
	}

	ret = pmem2_config_set_required_store_granularity(
		cfg, PMEM2_GRANULARITY_PAGE);
	if (ret) {
		fprintf(stderr,
			"pmem2_config_set_required_store_granularity: %s\n",
			pmem2_errormsg());
		goto err_config_set;
	}

	ret = pmem2_map(cfg, &ctx->map);
	if (ret) {
		fprintf(stderr, "pmem2_map: %s\n", pmem2_errormsg());
		goto err_config_set;
	}

	ret = pmem2_config_delete(&cfg);
	if (ret) {
		fprintf(stderr, "pmem2_config_delete: %s\n", pmem2_errormsg());
		goto err_config_new;
	}

	ctx->flush_fn = pmem2_get_flush_fn(ctx->map);
	ctx->drian_fn = pmem2_get_drain_fn(ctx->map);
	ctx->persist_fn = pmem2_get_persist_fn(ctx->map);

	return;

err_config_set:
	pmem2_config_delete(&cfg);
err_config_new:
	close(ctx->fd);
	exit(ret);
}

/*
 * elem_next -- return address of next element
 */
static struct aof_elem *
elem_next(struct aof_elem *elem)
{
	return (struct aof_elem *)((uintptr_t)elem + elem->elem_size);
}

/*
 * elem_first -- return address of the first element in AOF structure
 */
static struct aof_elem *
elem_first(struct aof_header *header)
{
	return (struct aof_elem *)((uintptr_t)header + sizeof(*header));
}

/*
 * aof_header_init -- init AOF header
 */
static struct aof_header *
aof_header_init(struct aof *ctx)
{
	struct aof_header *header = pmem2_map_get_address(ctx->map);
	ctx->persist_fn(header, sizeof(*header));
	if (!header->offset) {
		header->offset = sizeof(*header);
		ctx->persist_fn(header, sizeof(*header));
		header->n_elems = 0;
		ctx->persist_fn(header, sizeof(*header));
		header->tail = (void *)elem_first(header);
	}
	header->tail = (void *)((uintptr_t)header + header->offset);

	ctx->persist_fn(header, sizeof(*header));

	return header;
}

/*
 * checksum_calc -- calculate a checksum for the given element
 */
static size_t
checksum_calc(struct aof_elem *elem)
{
	size_t checksum = ((size_t)elem->id + (size_t)elem->pattern[0] +
				(size_t)elem->elem_size) % MAGIC_NUM;

	return checksum;
}

/*
 * update_header -- update AOF header
 */
static void
update_header(struct aof_header *header, size_t off, size_t n_elem,
		struct aof *ctx)
{
	header->offset = off;
	header->n_elems = n_elem;
	ctx->persist_fn(header, sizeof(*header));
}

/*
 * fill_element -- fill all elements in aof_elem structure
 */
static void
fill_element(struct aof_elem *elem, size_t id, size_t elem_size,
		size_t pattern_size)
{
	elem->id = id;
	elem->elem_size = elem_size;
	char v = src_pattern[elem->id % SRC_PATTERN_LEN];
	memset(elem->pattern, v, pattern_size - 1);
	elem->pattern[pattern_size] = '\0';
	elem->checksum = checksum_calc(elem);
}

/*
 * elem_create -- create new aof_elem structure
 */
static struct aof_elem *
elem_create(void *addr, size_t id, void *end_ptr)
{
	struct aof_elem *elem = addr;
	size_t pattern_size = rand() % MAX_WRITE_SIZE + 2;
	size_t elem_size =
		ALIGN_UP(pattern_size + sizeof(*elem), CACHELINE_SIZE);
	if ((void *)((uintptr_t)addr + elem_size) > end_ptr)
		return NULL;

	fill_element(elem, id, elem_size, pattern_size);

	return elem;
}

/*
 * append_n_elements -- add n elements to AOF structure
 */
static void
append_n_elements(struct aof_header *header, struct aof *ctx)
{
	void *addr = header->tail;
	size_t curr_id = 0;
	if (header->n_elems)
		curr_id = header->n_elems;

	struct aof_elem *elem = NULL;

	size_t used_memory = header->offset;
	/* Flushes number, after which will run drain_fn() */
	size_t flushes_before_drain = ctx->flush_stride;

	size_t map_size = pmem2_map_get_size(ctx->map);
	void *end_ptr = (void *)((uintptr_t)header + map_size);

	for (int i = 0; i < ctx->n_new_entries; ++i) {
		elem = elem_create(addr, curr_id, end_ptr);
		if (!elem)
			break;

		ctx->flush_fn(elem, elem->elem_size);
		--flushes_before_drain;
		used_memory += elem->elem_size;

		if (flushes_before_drain == 0)
			flushes_before_drain = ctx->flush_stride;

		addr = (void *)elem_next(elem);
		++curr_id;
	}
	ctx->drian_fn();
	update_header(header, used_memory, curr_id, ctx);
}

/*
 * print_all_elements -- print letters, whose represent all elements of to
 * AOF structure and validate these letters
 */
static void
print_all_elements(struct aof_header *header, struct aof ctx)
{

	struct aof_elem *curr_elem = elem_first(header);
	while ((void *)curr_elem < (void *)header->tail) {
		assert(curr_elem->pattern[0] ==
			src_pattern[curr_elem->id % SRC_PATTERN_LEN]);
		fprintf(stdout, "%c", curr_elem->pattern[0]);
		curr_elem = elem_next(curr_elem);
	}
	fprintf(stdout, "\n");
}

/*
 * validate_file -- check checksum of all elements in AOF structure
 */
static void
validate_file(struct aof_header *header, struct aof ctx)
{
	struct aof_elem *curr_elem = elem_first(header);
	size_t i = 0;
	while ((void *)curr_elem < (void *)header->tail) {
		size_t checksum = checksum_calc(curr_elem);
		assert(checksum == curr_elem->checksum);
		curr_elem = elem_next(curr_elem);
		++i;
	}
	assert(i == header->n_elems);
}

int
main(int argc, char *argv[])
{
	struct aof ctx = {0};

	parse_args(argc, argv, &ctx);

	aof_init(&ctx);

	struct aof_header *header = aof_header_init(&ctx);

	switch (ctx.mode) {
		case MODE_APPEND:
			append_n_elements(header, &ctx);
			break;

		case MODE_PRINT:
			print_all_elements(header, ctx);
			break;

		case MODE_VALIDATE:
			validate_file(header, ctx);
			break;

		default:
			return 1;
	}

	return 0;
}
