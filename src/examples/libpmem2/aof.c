// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * aof.c -- Suboptimal but "persistently-correct" implementation of
 * an append-only file structure. The user may do multiple appends and flush
 * them all. When an appendv command is done, the application drains all
 * previous flushes, updates the header (especially information about the end
 * of the valid data in the file), and persists the changes done to the header.
 */

#include <assert.h>
#include <errno.h>
#include <ex_common.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libpmem2.h>

#define PMEM2_AOF "Enough about you, let's talk about me, Johnny Bravo"
#define PMEM2_AOF_LEN (sizeof(PMEM2_AOF))

#define MIN_SIZE (1 << 20) /* 1 MiB */

/*
 * aof_header -- AOF header
 */
struct aof_header {
	uint8_t signature[PMEM2_AOF_LEN];
	uint64_t end_offset;
};

/*
 * aof -- append-only file application context
 */
struct aof {
	struct aof_header *header;
	struct pmem2_map *map;
	void *end_ptr;
	void *tail;
	/* pmem2 on Windows requires an open fd to flush buffers (non-pmem) */
	int fd;

	pmem2_drain_fn drain_fn;
	pmem2_persist_fn persist_fn;
	pmem2_memcpy_fn memcpy_fn;

	/* user arguments */
	char *path;
};

/*
 * aof_vec -- AOF vector
 */
struct aof_vec {
	uint8_t *addr;
	size_t len;
};

/*
 * aof_is_initialized -- check if aof header is initialized
 */
static int
aof_is_initialized(struct aof_header *header)
{
	return memcmp(header->signature, PMEM2_AOF, PMEM2_AOF_LEN) == 0;
}

/*
 * aof_rewind -- remove the AOF file content
 */
static int
aof_rewind(struct aof *ctx)
{
	struct aof_header *header = ctx->header;

	header->end_offset = sizeof(*header);
	ctx->persist_fn(&header->end_offset, sizeof(header->end_offset));

	ctx->tail = (void *)((uintptr_t)header + header->end_offset);

	return 0;
}

/*
 * aof_header_init -- init AOF header
 */
static void
aof_header_init(struct aof *ctx)
{
	(void) aof_rewind(ctx);

	struct aof_header *header = ctx->header;
	memcpy(header->signature, PMEM2_AOF, PMEM2_AOF_LEN);
	ctx->persist_fn(header->signature, PMEM2_AOF_LEN);
}

/*
 * aof_init -- prepare AOF application context
 */
static int
aof_init(struct aof *ctx)
{
	if ((ctx->fd = open(ctx->path, O_RDWR)) < 0) {
		perror("open");
		return 1;
	}

	struct pmem2_config *cfg;
	int ret = pmem2_config_new(&cfg);
	if (ret) {
		fprintf(stderr, "pmem2_config_new: %s\n", pmem2_errormsg());
		goto err_config_new;
	}

	ret = pmem2_config_set_required_store_granularity(
		cfg, PMEM2_GRANULARITY_PAGE);
	if (ret) {
		fprintf(stderr,
			"pmem2_config_set_required_store_granularity: %s\n",
			pmem2_errormsg());
		goto err_cfg_set_req_granularity;
	}

	struct pmem2_source *src;
	ret = pmem2_source_from_fd(&src, ctx->fd);
	if (ret) {
		fprintf(stderr, "pmem2_config_set_fd: %s\n", pmem2_errormsg());
		goto err_source;
	}

	ret = pmem2_map(cfg, src, &ctx->map);
	if (ret) {
		fprintf(stderr, "pmem2_map: %s\n", pmem2_errormsg());
		goto err_map;
	}

	size_t map_size = pmem2_map_get_size(ctx->map);
	if (map_size < MIN_SIZE) {
		fprintf(stderr, "aof_init: Not enough space in the file\n");
		goto err_map_size;
	}

	ret = pmem2_source_delete(&src);
	if (ret != 0 || src != NULL) {
		fprintf(stderr, "pmem2_source_delete: %s\n", pmem2_errormsg());
		(void) pmem2_unmap(&ctx->map);
		(void) pmem2_config_delete(&cfg);
		(void) close(ctx->fd);
		return ret;
	}

	ret = pmem2_config_delete(&cfg);
	if (ret) {
		fprintf(stderr, "pmem2_config_delete: %s\n", pmem2_errormsg());
		(void) pmem2_unmap(&ctx->map);
		(void) pmem2_source_delete(&src);
		(void) close(ctx->fd);
		return ret;
	}

	ctx->drain_fn = pmem2_get_drain_fn(ctx->map);
	ctx->persist_fn = pmem2_get_persist_fn(ctx->map);
	ctx->memcpy_fn = pmem2_get_memcpy_fn(ctx->map);

	ctx->header = pmem2_map_get_address(ctx->map);
	if (!aof_is_initialized(ctx->header))
		aof_header_init(ctx);

	ctx->end_ptr = (void *)((uintptr_t)ctx->header + map_size);
	ctx->tail = (void *)((uintptr_t)ctx->header + ctx->header->end_offset);

	return ret;

err_map_size:
	(void) pmem2_unmap(&ctx->map);
err_map:
	(void) pmem2_source_delete(&src);
err_source:
err_cfg_set_req_granularity:
	(void) pmem2_config_delete(&cfg);
err_config_new:
	(void) close(ctx->fd);
	return ret;
}

/*
 * aof_init -- cleanup AOF application context
 */
static int
aof_fini(struct aof *ctx)
{
	int ret = pmem2_unmap(&ctx->map);
	if (ret) {
		fprintf(stderr, "pmem2_unmap: %s\n", pmem2_errormsg());
		return ret;
	}

	ret = close(ctx->fd);
	if (ret) {
		perror("close");
		return ret;
	}

	return ret;
}

/*
 * aof_header_update -- update AOF header
 */
static void
aof_header_update(struct aof *ctx, size_t off)
{
	struct aof_header *header = ctx->header;

	header->end_offset += off;
	ctx->persist_fn(&header->end_offset, sizeof(header->end_offset));

	/* tail depends on the header that's why is updated in this place */
	ctx->tail = (void *)((uintptr_t)ctx->tail + off);
}

/*
 * aof_append -- append a new string to AOF
 */
static int
aof_append(struct aof *ctx, uint8_t *data, size_t data_len)
{
	void *addr = ctx->tail;
	if ((void *)((uintptr_t)addr + data_len) > ctx->end_ptr) {
		fprintf(stderr, "aof_append: no space left in the file\n");
		return -1;
	}

	memcpy(addr, (char *)data, data_len);
	ctx->persist_fn(addr, data_len);

	aof_header_update(ctx, data_len);

	return 1;
}

/*
 * aof_set_vec -- allocate and fill aof_vec structure
 */
static int
aof_set_vec(struct aof_vec **vec, uint8_t *addr, size_t aof_vcnt)
{
	int i = 0;
	for (; i < aof_vcnt; ++i) {
		vec[i] = malloc(sizeof(struct aof_vec));
		if (!vec[i])
			goto err_malloc;

		vec[i]->addr = addr;
		vec[i]->len = strlen((char *)addr);
		addr = (uint8_t *)((uintptr_t)vec[i]->addr + vec[i]->len + 1);
	}

	return 0;

err_malloc:
	for (; i >= 0; --i) {
		free(vec[i]);
	}
	return 1;
}

/*
 * aof_free_vec -- free aof_vec structure
 */
static void
aof_free_vec(struct aof_vec **vec, size_t aof_vcnt)
{
	for (int i = 0; i < aof_vcnt; ++i) {
		free(vec[i]);
	}
	free(vec);
}

/*
 * aof_appendv -- append N new strings to AOF and return the number
 * of consumed arguments
 */
static int
aof_appendv(struct aof *ctx, struct aof_vec *aofv[], size_t aofvcnt)
{
	void *addr = ctx->tail;
	size_t total_len = 0;

	for (int i = 0; i < aofvcnt; ++i) {
		if ((void *)((uintptr_t)addr + aofv[i]->len) > ctx->end_ptr) {
			fprintf(stderr,
				"aof_appendv: no space left in the file\n");
			return -1;
		}

		ctx->memcpy_fn(addr, aofv[i]->addr, aofv[i]->len,
				PMEM2_F_MEM_NODRAIN);

		total_len += aofv[i]->len;
		addr = (void *)((uintptr_t)addr + aofv[i]->len);
	}

	ctx->drain_fn();

	aof_header_update(ctx, total_len);

	return aofvcnt + 1;
}

/*
 * aof_dump -- dump the AOF file content
 */
static int
aof_dump(struct aof *ctx)
{
	char *start = (char *)((uintptr_t)ctx->header + sizeof(*ctx->header));
	size_t total_len = (size_t)((uintptr_t)ctx->tail - (uintptr_t)start);
	fwrite(start, sizeof(char), total_len, stdout);
	printf("\n");

	return 0;
}

/*
 * print_usage -- print short description of usage
 */
static void
print_usage(void)
{
	printf("Usage:\n"
		"\taof <file> <COMMAND_1> [COMMAND_2 ...]\n"
		"\taof help\n"
		"Available commands:\n"
		"append DATA\t\t\t- add a new element to the AOF\n"
		"appendv N DATA_1 ... DATA_N\t- add N new elements to the AOF\n"
		"rewind\t\t\t\t- remove the AOF content\n"
		"dump\t\t\t\t- dump the file content to the console\n"
		"help\t\t\t\t- print this help info\n");
}

int
main(int argc, char *argv[])
{
	if (argv[1] && strcmp(argv[1], "help") == 0) {
		print_usage();
		return 0;
	} else if (argc < 3) {
		print_usage();
		return 1;
	}

	struct aof ctx = {0};

	ctx.path = argv[1];

	int ret = aof_init(&ctx);
	if (ret)
		return ret;

	/* skip executable name and the AOF file name */
	argc -= 2;
	argv += 2;
	while (argc > 0) {
		if (strcmp(argv[0], "append") == 0 && argc > 1) {
			uint8_t *data = (uint8_t *)argv[1];
			size_t data_len = strlen((char *)data);

			ret = aof_append(&ctx, data, data_len);
		} else if (strcmp(argv[0], "appendv") == 0 && argc > 1) {
			errno = 0;
			char *end;
			size_t aof_vcnt = strtoul(argv[1], &end, 0);
			if ((aof_vcnt == 0) || (errno != 0 && *end == '\0')) {
				fprintf(stderr,
					"aof_appendv: invalid N argument: %s\n",
					argv[1]);
				ret = 1;
				goto err_out;
			}
			/* number of needed arguments: 2 + aof_vcnt */
			if (argc < 2 + aof_vcnt) {
				fprintf(stderr,
					"aof_appendv: a too small number of strings provided\n");
				ret = 1;
				goto err_out;
			}
			struct aof_vec **vec;
			vec = malloc(sizeof(struct aof_vec) * aof_vcnt);
			if (!vec) {
				perror("malloc");
				ret = 1;
				goto err_out;
			}
			uint8_t *addr = (uint8_t *)argv[2];
			ret = aof_set_vec(vec, addr, aof_vcnt);
			if (ret) {
				free(vec);
				goto err_out;
			}

			ret = aof_appendv(&ctx, vec, aof_vcnt);

			aof_free_vec(vec, aof_vcnt);
		} else if (strcmp(argv[0], "dump") == 0) {
			ret = aof_dump(&ctx);
		} else if (strcmp(argv[0], "rewind") == 0) {
			ret = aof_rewind(&ctx);
		} else {
			fprintf(stderr,
				"aof: %s - unknown command or a too small number of arguments\n",
				argv[0]);
			print_usage();
			ret = -1;
		}

		if (ret < 0) {
			ret = 1;
			goto err_out;
		} else {
			/*
			 * One argument consumed by the name of the performed
			 * operation.
			 */
			ret += 1;
			argc -= ret;
			argv += ret;
			ret = 0;
		}
	}

	ret = aof_fini(&ctx);

	return ret;

err_out:
	(void) aof_fini(&ctx);

	return ret;
}
