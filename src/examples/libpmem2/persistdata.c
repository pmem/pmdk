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
 * the valid data in the file) and persists the changes done to the header.
 */

#include <errno.h>
#include <ex_common.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libpmem2.h>

#define AOF_SIGNATURE "Enough about you, let's talk about me, Johnny Bravo"
#define AOF_SIGNATURE_LEN (sizeof(AOF_SIGNATURE))

#define MIN_SIZE 1 << 20

/*
 * aof_header -- AOF header
 */
struct aof_header {
	uint64_t signature[AOF_SIGNATURE_LEN];
	uint64_t end_offset;
};

/*
 * aof -- append-only file application context
 */
struct aof {
	struct aof_header *header;
	struct pmem2_map *map;
	void *tail;

	pmem2_flush_fn flush;
	pmem2_drain_fn drain;
	pmem2_persist_fn persist;

	/* user arguments */
	char *path;
	char *command;
};

/*
 * print_usage -- print short description of usage
 */
static void
print_usage(void)
{
	printf("Usage:\taof <file> <COMMAND_1> [COMMAND_2 ...]\n"
		"\taof help\n"
		"Available commands:\n"
		"append DATA\t\t\t- add a new element to the AOF\n"
		"appendv N DATA_1 ... DATA_N\t- add N new elements to the AOF\n"
		"rewind\t\t\t\t- remove the AOF content\n"
		"dump\t\t\t\t- dump the file content to the console\n"
		"help\t\t\t\t- print this help info\n");
}

/*
 * aof_init -- prepare AOF application context
 */
static void
aof_init(struct aof *ctx)
{
	int fd;
	if ((fd = open(ctx->path, O_RDWR)) < 0) {
		perror("open");
		exit(1);
	}

	struct pmem2_config *cfg;
	struct pmem2_source *src;
	int ret = pmem2_config_new(&cfg);
	if (ret) {
		fprintf(stderr, "pmem2_config_new: %s\n", pmem2_errormsg());
		goto err_config_new;
	}

	ret = pmem2_source_from_fd(&src, fd);
	if (ret) {
		fprintf(stderr, "pmem2_config_set_fd: %s\n", pmem2_errormsg());
		goto err_cfg_set_fd;
	}

	ret = pmem2_config_set_required_store_granularity(
		cfg, PMEM2_GRANULARITY_PAGE);
	if (ret) {
		fprintf(stderr,
			"pmem2_config_set_required_store_granularity: %s\n",
			pmem2_errormsg());
		goto err_cfg_set_req_granularity;
	}

	ret = pmem2_map(cfg, src, &ctx->map);
	if (ret) {
		fprintf(stderr, "pmem2_map: %s\n", pmem2_errormsg());
		goto err_map;
	}

	if (pmem2_map_get_size(ctx->map) < MIN_SIZE) {
		fprintf(stderr, "aof_init: Not enough space in the file\n");
		goto err_map_size;
	}

	ret = pmem2_source_delete(&src);
	if (ret || src != NULL) {
		fprintf(stderr, "pmem2_source_delete: %s\n", pmem2_errormsg());
		goto err_source_delete;
	}

	ret = pmem2_config_delete(&cfg);
	if (ret) {
		fprintf(stderr, "pmem2_config_delete: %s\n", pmem2_errormsg());
		(void) pmem2_source_delete(&src);
		(void) pmem2_unmap(&ctx->map);
		(void) close(fd);
		exit(ret);
	}

	ctx->flush = pmem2_get_flush_fn(ctx->map);
	ctx->drain = pmem2_get_drain_fn(ctx->map);
	ctx->persist = pmem2_get_persist_fn(ctx->map);

	close(fd);

	return;

err_source_delete:
err_map_size:
	(void) pmem2_unmap(&ctx->map);
err_map:
err_cfg_set_req_granularity:
err_cfg_set_fd:
	(void) pmem2_config_delete(&cfg);
err_config_new:
	(void) close(fd);
	exit(ret);
}

/*
 * aof_init -- cleanup AOF application context
 */
static void
aof_fini(struct aof *ctx)
{
	int ret = pmem2_unmap(&ctx->map);
	if (ret) {
		fprintf(stderr, "pmem2_unmap: %s\n", pmem2_errormsg());
		exit(1);
	}
}

/*
 * aof_is_initialized -- check if aof header is initialized
 */
static int
aof_is_initialized(struct aof_header *header)
{
	return memcmp(header->signature, AOF_SIGNATURE, AOF_SIGNATURE_LEN) == 0;
}

/*
 * aof_header_init -- init AOF header
 */
static void
aof_header_init(struct aof *ctx)
{
	ctx->header->end_offset = sizeof(*ctx->header);
	ctx->persist(&ctx->header->end_offset, sizeof(ctx->header->end_offset));

	memcpy(ctx->header->signature, AOF_SIGNATURE, AOF_SIGNATURE_LEN);
	ctx->persist(&ctx->header->signature, AOF_SIGNATURE_LEN);

	/* tail depends on the header that's why is updated in this place */
	ctx->tail = (void *)((uintptr_t)ctx->header + ctx->header->end_offset);
}

/*
 * aof_header_update -- update AOF header
 */
static void
aof_header_update(size_t off, struct aof *ctx)
{
	ctx->header->end_offset += off;
	ctx->persist(&ctx->header->end_offset, sizeof(ctx->header->end_offset));

	/* tail depends on the header that's why is updated in this place */
	ctx->tail = (void *)((uintptr_t)ctx->header + ctx->header->end_offset);
}

/*
 * aof_append -- append a new string to AOF
 */
static int
aof_append(char *argv[], struct aof *ctx)
{
	char *data = *argv;

	void *addr = ctx->tail;
	size_t map_size = pmem2_map_get_size(ctx->map);
	void *end_ptr = (void *)((uintptr_t)ctx->header + map_size);
	size_t len = strlen(data);
	if ((void *)((uintptr_t)addr + len) > end_ptr) {
		fprintf(stderr, "aof_append: no space left in the file");
		return -1;
	}

	memcpy(addr, data, len);
	ctx->persist(addr, len);

	aof_header_update(len, ctx);

	return 1;
}

/*
 * aof_appendv -- append N new strings to AOF
 */
static int
aof_appendv(int argc, char *argv[], struct aof *ctx)
{
	size_t n_new_strings = strtoul(argv[0], NULL, 0);
	if ((!n_new_strings) ||
		(n_new_strings == ULONG_MAX && errno == ERANGE)) {
		fprintf(stderr, "aof_appendv: invalid N argument: %s\n",
			argv[0]);
		return -1;
	}

	/* n_args - number of needed arguments (1 + number of strings) */
	size_t n_args = n_new_strings + 1;
	if (argc < n_args) {
		fprintf(stderr,
			"aof_appendv: a too small number of strings provided\n");
		return -1;
	}

	void *addr = ctx->tail;
	size_t map_size = pmem2_map_get_size(ctx->map);
	void *end_ptr = (void *)((uintptr_t)ctx->header + map_size);
	size_t total_len = 0;
	size_t len;
	char *data = NULL;

	unsigned arg_id;
	for (arg_id = 1; arg_id <= n_new_strings; ++arg_id) {
		data = argv[arg_id];
		len = strlen(data);
		if ((void *)((uintptr_t)addr + len) > end_ptr) {
			fprintf(stderr,
				"aof_appendv: no space left in the file\n");
			return -1;
		}

		memcpy(addr, data, len);
		ctx->flush(addr, len);

		total_len += len;
		addr = (void *)((uintptr_t)addr + len);
	}
	ctx->drain();
	aof_header_update(total_len, ctx);

	return n_args;
}

/*
 * aof_dump -- dump the AOF file content
 */
static int
aof_dump(struct aof *ctx)
{
	char *elem = (void *)((uintptr_t)ctx->header + sizeof(*ctx->header));
	size_t total_len = (size_t)((uintptr_t)ctx->tail - (uintptr_t)elem);
	fwrite(elem, 1, total_len, stdout);
	printf("\n");

	return 0;
}

/*
 * aof_rewind -- remove the AOF file content
 */
static int
aof_rewind(struct aof *ctx)
{
	ctx->header->end_offset = sizeof(*ctx->header);
	ctx->persist(&ctx->header->end_offset, sizeof(ctx->header->end_offset));

	ctx->tail = (void *)((uintptr_t)ctx->header + ctx->header->end_offset);

	return 0;
}

int
main(int argc, char *argv[])
{
	if (strcmp(argv[1], "help") == 0) {
		print_usage();
		exit(0);
	} else if (argc < 3) {
		print_usage();
		exit(1);
	}

	struct aof ctx = {0};

	ctx.path = argv[1];

	aof_init(&ctx);

	ctx.header = pmem2_map_get_address(ctx.map);
	if (!aof_is_initialized(ctx.header))
		aof_header_init(&ctx);

	ctx.tail = (void *)((uintptr_t)ctx.header + ctx.header->end_offset);

	int ret;
	argc -= 2;
	argv += 2;
	while (argc > 0) {
		if (strcmp(argv[0], "append") == 0 && argc > 1) {
			ret = aof_append((&argv[1]), &ctx);
		} else if (strcmp(argv[0], "appendv") == 0) {
			ret = aof_appendv(argc, &argv[1], &ctx);
		} else if (strcmp(argv[0], "dump") == 0) {
			ret = aof_dump(&ctx);
		} else if (strcmp(argv[0], "rewind") == 0) {
			ret = aof_rewind(&ctx);
		} else {
			fprintf(stderr,
				"aof: %s - unknown command or or a too small number of arguments\n",
				argv[0]);
			print_usage();
			ret = -1;
		}

		if (ret < 0) {
			ret = 1;
			break;
		} else {
			/*
			 * One argument consumed by the name of the performed
			 * operation.
			 */
			ret += 1;
			argc -= ret;
			argv += ret;
		}
		ret = 0;
	}

	aof_fini(&ctx);

	return ret;
}
