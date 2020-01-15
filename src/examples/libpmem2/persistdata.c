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
 * aof -- append-only file application context
 */
struct aof {
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
 * aof_header -- AOF header
 */
struct aof_header {
	uint64_t signature[AOF_SIGNATURE_LEN];
	uint64_t end_offset;
};

/*
 * print_usage -- print short description of usage
 */
static void
print_usage(void)
{
	printf("Usage:\taof <file> append \"STR\"\n"
		"\taof <file> appendv N \"STR_1\" \"STR_2\" ... \"STR_N\"\n"
		"\taof <file> rewind\n"
		"\taof <file> walk\n"
		"\taof help\n"
		"Available commnads:\n"
		"append DATA\t\t\t- add new element to the <file>\n"
		"appendv N DATA ... DATA_N\t- add N elements to the <file>\n"
		"rewind\t\t\t\t- reset the current write point to the beginning\n"
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
	int ret = pmem2_config_new(&cfg);
	if (ret) {
		fprintf(stderr, "pmem2_config_new: %s\n", pmem2_errormsg());
		goto err_config_new;
	}

	ret = pmem2_config_set_fd(cfg, fd);
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

	ret = pmem2_map(cfg, &ctx->map);
	if (ret) {
		fprintf(stderr, "pmem2_map: %s\n", pmem2_errormsg());
		goto err_map;
	}

	if (pmem2_map_get_size(ctx->map) < MIN_SIZE) {
		fprintf(stderr, "aof_init: Not enough space in the file\n");
		goto err_map_size;
	}

	ret = pmem2_config_delete(&cfg);
	if (ret) {
		fprintf(stderr, "pmem2_config_delete: %s\n", pmem2_errormsg());
		goto err_config_delete;
	}

	ctx->flush = pmem2_get_flush_fn(ctx->map);
	ctx->drain = pmem2_get_drain_fn(ctx->map);
	ctx->persist = pmem2_get_persist_fn(ctx->map);

	close(fd);

	return;

err_config_delete:
err_map_size:
	pmem2_unmap(&ctx->map);
err_cfg_set_req_granularity:
err_cfg_set_fd:
err_map:
	(void) pmem2_config_delete(&cfg);
err_config_new:
	close(fd);
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
aof_header_init(struct aof_header *header, struct aof *ctx)
{
	memcpy(header->signature, AOF_SIGNATURE, AOF_SIGNATURE_LEN);
	ctx->persist(&header->signature, AOF_SIGNATURE_LEN);

	header->end_offset = sizeof(*header);
	ctx->persist(&header->end_offset, sizeof(header->end_offset));
}

/*
 * aof_header_update -- update AOF header
 */
static void
aof_header_update(struct aof_header *header, size_t off, struct aof *ctx)
{
	header->end_offset += off;
	ctx->persist(&header->end_offset, sizeof(header->end_offset));
}

/*
 * aof_append -- append new string to AOF
 */
static int
aof_append(int argc, char *argv, struct aof_header *header, struct aof *ctx)
{
	char *data = argv;

	if (!data)
		return -1;

	void *addr = ctx->tail;
	size_t map_size = pmem2_map_get_size(ctx->map);
	void *end_ptr = (void *)((uintptr_t)header + map_size);
	size_t len = strlen(data);
	if ((void *)((uintptr_t)addr + len) > end_ptr) {
		fprintf(stderr, "aof_append: No space left in the file");
		return -1;
	}

	memcpy(addr, data, len);
	ctx->persist(addr, len);

	aof_header_update(header, len, ctx);

	return 2;
}

/*
 * aof_appendv -- append N new strings to AOF
 */
static int
aof_appendv(int argc, char *argv[], struct aof_header *header, struct aof *ctx)
{
	size_t n_new_strings = strtoul(*argv, NULL, 0);
	++argv;
	if ((!n_new_strings) ||
	    (n_new_strings == ULONG_MAX && errno == ERANGE)) {
		fprintf(stderr, "aof_appendv: wrong N argument\n");
		return -1;
	}

	/* n_args - number of needed arguments (N + number of strings) */
	size_t n_args = n_new_strings + 1;
	if (argc < n_args) {
		fprintf(stderr, "aof_appendv: Wrong number of strings\n");
		return -1;
	}

	void *addr = ctx->tail;
	size_t map_size = pmem2_map_get_size(ctx->map);
	void *end_ptr = (void *)((uintptr_t)header + map_size);
	size_t strings_len = 0;
	size_t len;
	char *data = NULL;

	unsigned i;
	for (i = 0; i < n_new_strings; ++i) {
		data = *argv;
		argv += 1;
		if (!data) {
			fprintf(stderr, "aof_appendv: Invalid data\n");
			return -1;
		}

		len = strlen(data);
		if ((void *)((uintptr_t)addr + len) > end_ptr) {
			fprintf(stderr,
				"aof_appendv: No space left in the file\n");
			return -1;
		}

		memcpy(addr, data, len);
		ctx->flush(addr, len);

		strings_len += len;
		addr = (void *)((uintptr_t)addr + len);
	}
	ctx->drain();
	aof_header_update(header, strings_len, ctx);

	return (n_args + 1 /* + 1 argument (command name) */);
}

/*
 * aof_dump -- dump the AOF file content
 */
static int
aof_dump(struct aof_header *header, struct aof *ctx)
{
	char *elem = (void *)((uintptr_t)header + sizeof(*header));
	fwrite(elem, 1, strlen(elem), stdout);
	printf("\n");

	return 1;
}

/*
 * aof_rewind -- reset the current end offset to the length of the header
 */
static int
aof_rewind(struct aof_header *header, struct aof *ctx)
{
	header->end_offset = sizeof(*header);
	ctx->persist(&header->end_offset, sizeof(header->end_offset));

	ctx->tail = (void *)((uintptr_t)header + header->end_offset);

	return 1;
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

	struct aof_header *header = pmem2_map_get_address(ctx.map);
	if (!aof_is_initialized(header))
		aof_header_init(header, &ctx);

	ctx.tail = (void *)((uintptr_t)header + header->end_offset);

	int ret;
	argv += 2;
	while (argc > 2) {
		ctx.command = *argv;
		if (strcmp(ctx.command, "append") == 0) {
			ret = aof_append(argc, *(argv + 1), header, &ctx);
		} else if (strcmp(ctx.command, "appendv") == 0) {
			ret = aof_appendv(argc, (argv + 1), header, &ctx);
		} else if (strcmp(ctx.command, "dump") == 0) {
			ret = aof_dump(header, &ctx);
		} else if (strcmp(ctx.command, "rewind") == 0) {
			ret = aof_rewind(header, &ctx);
		} else {
			fprintf(stderr, "aof_appendv: %s - unknown command\n",
				ctx.command);
			print_usage();
			ret = -1;
		}

		if (ret < 0) {
			ret = 1;
			break;
		} else {
			argc -= ret;
			argv += ret;
		}
		ret = 0;
	}

	aof_fini(&ctx);

	return ret;
}
