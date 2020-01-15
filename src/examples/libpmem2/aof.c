// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * aof.c -- "Persistently-correct" implementation of an append-only file
 * structure. The user may do single/multiple appends and flush them all.
 * When an append/appendv command is done, the application persists all append
 * data, updates the header (especially information about the end of the valid
 * data in the file), and persists the changes done to the header.
 */

#include <errno.h>
#include <ex_common.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libpmem2.h>

#define AOF_HDR_SIGNATURE "PMEM2_AOF"
#define AOF_HDR_SIGNATURE_LEN (sizeof(AOF_HDR_SIGNATURE))

#define MIN_SIZE (1U << 20) /* 1 MiB */

/*
 * aof_header -- AOF header
 */
struct aof_header {
	uint8_t signature[AOF_HDR_SIGNATURE_LEN];
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
	/*
	 * pmem2 on non-DAX Windows volumes requires an open fd to flush buffers
	 */
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
	return memcmp(header->signature, AOF_HDR_SIGNATURE,
		AOF_HDR_SIGNATURE_LEN) == 0;
}

/*
 * aof_header_update -- update AOF header
 */
static void
aof_header_update(struct aof *ctx, int64_t off)
{
	struct aof_header *header = ctx->header;

	header->end_offset += off;
	ctx->persist_fn(&header->end_offset, sizeof(header->end_offset));

	/* tail depends on the header that's why is updated in this place */
	ctx->tail = (void *)((uintptr_t)ctx->tail + off);
}

/*
 * aof_rewind -- remove the AOF file content
 */
static void
aof_rewind(struct aof *ctx)
{
	int64_t end_header = sizeof(*ctx->header) - ctx->header->end_offset;
	aof_header_update(ctx, end_header);
}

/*
 * aof_header_init -- init AOF header
 */
static void
aof_header_init(struct aof *ctx)
{
	aof_rewind(ctx);

	struct aof_header *header = ctx->header;
	memcpy(header->signature, AOF_HDR_SIGNATURE, AOF_HDR_SIGNATURE_LEN);
	ctx->persist_fn(header->signature, AOF_HDR_SIGNATURE_LEN);
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
		pmem2_perror("pmem2_config_new");
		goto err_config_new;
	}

	ret = pmem2_config_set_required_store_granularity(
		cfg, PMEM2_GRANULARITY_PAGE);
	if (ret) {
		pmem2_perror("pmem2_config_set_required_store_granularity");
		goto err_cfg_set_req_granularity;
	}

	struct pmem2_source *src;
	ret = pmem2_source_from_fd(&src, ctx->fd);
	if (ret) {
		pmem2_perror("pmem2_config_set_fd");
		goto err_source;
	}

	ret = pmem2_map(cfg, src, &ctx->map);
	if (ret) {
		pmem2_perror("pmem2_map");
		goto err_map;
	}

	size_t map_size = pmem2_map_get_size(ctx->map);
	if (map_size < MIN_SIZE) {
		fprintf(stderr, "aof_init: not enough space in the file\n");
		ret = 1;
		goto err_map_size;
	}

	ret = pmem2_source_delete(&src);
	if (ret) {
		pmem2_perror("pmem2_source_delete");
		goto err_source_delete;
	}

	ret = pmem2_config_delete(&cfg);
	if (ret) {
		pmem2_perror("pmem2_config_delete");
		goto err_cfg_delete;
	}

	ctx->drain_fn = pmem2_get_drain_fn(ctx->map);
	ctx->persist_fn = pmem2_get_persist_fn(ctx->map);
	ctx->memcpy_fn = pmem2_get_memcpy_fn(ctx->map);

	ctx->header = pmem2_map_get_address(ctx->map);
	if (!aof_is_initialized(ctx->header))
		aof_header_init(ctx);

	if (map_size <= ctx->header->end_offset) {
		fprintf(stderr, "aof_init: no available space in the file\n");
		ret = 1;
		goto err_map_size;
	}

	ctx->end_ptr = (void *)((uintptr_t)ctx->header + map_size);
	ctx->tail = (void *)((uintptr_t)ctx->header + ctx->header->end_offset);

	return ret;

err_cfg_delete:
err_source_delete:
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
 * aof_fini -- cleanup AOF application context
 */
static int
aof_fini(struct aof *ctx)
{
	int ret = pmem2_unmap(&ctx->map);
	if (ret) {
		pmem2_perror("pmem2_unmap");
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
 * aof_append -- append a new string to AOF
 */
static int
aof_append(struct aof *ctx, uint8_t *data, size_t data_len)
{
	char *addr = ctx->tail;
	if ((addr + data_len) > (char *)ctx->end_ptr) {
		fprintf(stderr, "aof_append: no space left in the file\n");
		return -1;
	}

	ctx->memcpy_fn(addr, (char *)data, data_len, 0);

	aof_header_update(ctx, (int64_t)data_len);

	return 1;
}

/*
 * aof_appendv -- append N new strings to AOF and return the number
 * of consumed arguments
 */
static int
aof_appendv(struct aof *ctx, struct aof_vec *aofv, size_t aofvcnt)
{
	char *addr = ctx->tail;
	size_t total_len = 0;

	for (size_t i = 0; i < aofvcnt; ++i) {
		if ((addr + aofv->len) > (char *)ctx->end_ptr) {
			fprintf(stderr,
				"aof_appendv: no space left in the file\n");
			return -1;
		}

		ctx->memcpy_fn(addr, aofv->addr, aofv->len,
				PMEM2_F_MEM_NOFLUSH);

		total_len += aofv->len;
		addr += aofv->len;
		aofv++;
	}

	ctx->persist_fn(ctx->tail, total_len);

	aof_header_update(ctx, (int64_t)total_len);

	return (int)aofvcnt + 1;
}

/*
 * aof_dump -- dump the AOF file content
 */
static void
aof_dump(struct aof *ctx)
{
	char *start = (char *)ctx->header + sizeof(*ctx->header);
	size_t total_len = (size_t)((uintptr_t)ctx->tail - (uintptr_t)start);
	fwrite(start, sizeof(char), total_len, stdout);
	printf("\n");
}

/*
 * parse_aof_vec -- allocate and fill aof_vec structure
 */
static int
parse_aof_vec(struct aof_vec *vec, uint8_t *addr, size_t aof_vcnt)
{
	for (size_t i = 0; i < aof_vcnt; ++i) {
		vec[i].addr = addr;
		vec[i].len = strlen((char *)addr);
		addr += vec[i].len + 1;
	}

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
	}

	if (argc < 3) {
		print_usage();
		return 1;
	}

	struct aof ctx;

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
			struct aof_vec *vec;
			vec = malloc(sizeof(struct aof_vec) * aof_vcnt);
			if (!vec) {
				perror("malloc");
				ret = 1;
				goto err_out;
			}
			uint8_t *addr = (uint8_t *)argv[2];
			ret = parse_aof_vec(vec, addr, aof_vcnt);
			if (ret) {
				free(vec);
				goto err_out;
			}

			ret = aof_appendv(&ctx, vec, aof_vcnt);

			free(vec);
		} else if (strcmp(argv[0], "dump") == 0) {
			aof_dump(&ctx);
		} else if (strcmp(argv[0], "rewind") == 0) {
			aof_rewind(&ctx);
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
