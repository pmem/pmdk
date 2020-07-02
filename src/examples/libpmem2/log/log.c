// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * log.c -- "Persistently-correct" implementation of a log structure.
 * The user may do single/multiple appends and flush them all.
 * When an append/appendv command is done, the application persists all append
 * data, updates the header (this includes information about the end of
 * the valid data in the file), and persists the changes done to the header.
 */

#include <errno.h>
#include <ex_common.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libpmem2.h>

#define LOG_HDR_SIGNATURE "PMEM2_LOG"
#define LOG_HDR_SIGNATURE_LEN (sizeof(LOG_HDR_SIGNATURE))

#define MIN_SIZE (1U << 20) /* 1 MiB */

struct log_header {
	uint8_t signature[LOG_HDR_SIGNATURE_LEN];
	uint64_t used;
};

struct log {
	struct log_header header;
	uint8_t data[];
};

struct log_ctx {
	struct log *log;
	struct pmem2_map *map;
	size_t capacity;

	/*
	 * pmem2 on non-DAX Windows volumes requires an open fd to flush buffers
	 */
	int fd;

	pmem2_drain_fn drain_fn;
	pmem2_persist_fn persist_fn;
	pmem2_memcpy_fn memcpy_fn;
};

struct log_vec {
	uint8_t *addr;
	size_t len;
};

/*
 * log_is_initialized -- check if log header is initialized
 */
static int
log_is_initialized(struct log_header *header)
{
	return memcmp(header->signature, LOG_HDR_SIGNATURE,
		LOG_HDR_SIGNATURE_LEN) == 0;
}

/*
 * log_header_update -- update LOG header
 */
static void
log_header_update(struct log_ctx *ctx, uint64_t off)
{
	uint64_t *used = &ctx->log->header.used;

	*used += off;
	ctx->persist_fn(used, sizeof(*used));
}

/*
 * log_rewind -- remove the LOG file content
 */
static void
log_rewind(struct log_ctx *ctx)
{
	uint64_t *used = &ctx->log->header.used;

	*used = 0;
	ctx->persist_fn(used, sizeof(*used));
}

/*
 * log_header_init -- init LOG header
 */
static void
log_header_init(struct log_ctx *ctx)
{
	log_rewind(ctx);

	struct log_header *header = &ctx->log->header;
	memcpy(header->signature, LOG_HDR_SIGNATURE, LOG_HDR_SIGNATURE_LEN);
	ctx->persist_fn(header->signature, LOG_HDR_SIGNATURE_LEN);
}

/*
 * log_init -- prepare LOG application context
 */
static int
log_init(struct log_ctx *ctx, const char *path)
{
	ctx->fd = open(path, O_RDWR);
	if (ctx->fd < 0) {
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

	ret = pmem2_map(&ctx->map, cfg, src);
	if (ret) {
		pmem2_perror("pmem2_map");
		goto err_map;
	}

	size_t map_size = pmem2_map_get_size(ctx->map);
	if (map_size < MIN_SIZE) {
		fprintf(stderr, "log_init: not enough space in the file\n");
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

	ctx->log = pmem2_map_get_address(ctx->map);
	if (!log_is_initialized(&ctx->log->header))
		log_header_init(ctx);

	ctx->capacity = map_size - sizeof(struct log_header);

	if (ctx->log->header.used == ctx->capacity) {
		fprintf(stderr, "log_init: log is full\n");
		ret = 1;
		goto err_map_size;
	}

	if (ctx->log->header.used > ctx->capacity) {
		fprintf(stderr, "log_init: file truncated?\n");
		ret = 1;
		goto err_map_size;
	}

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
 * log_fini -- cleanup LOG application context
 */
static int
log_fini(struct log_ctx *ctx)
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
 * log_append -- append a new string to LOG
 */
static int
log_append(struct log_ctx *ctx, uint8_t *data, size_t data_len)
{
	struct log *log = ctx->log;
	if (log->header.used + data_len > ctx->capacity) {
		fprintf(stderr, "log_append: no space left in the file\n");
		return -1;
	}

	ctx->memcpy_fn(&log->data[log->header.used], (char *)data, data_len, 0);

	log_header_update(ctx, data_len);

	return 1;
}

/*
 * log_appendv -- append N new strings to LOG and return the number
 * of consumed arguments
 */
static int
log_appendv(struct log_ctx *ctx, struct log_vec *logv, size_t logvcnt)
{
	struct log *log = ctx->log;
	uint8_t *start = &log->data[log->header.used];
	uint8_t *addr = start;
	size_t total_len = 0;

	for (size_t i = 0; i < logvcnt; ++i) {
		if (log->header.used + logv->len > ctx->capacity) {
			fprintf(stderr,
				"log_appendv: no space left in the file\n");
			return -1;
		}

		ctx->memcpy_fn(addr, logv->addr, logv->len,
				PMEM2_F_MEM_NOFLUSH);

		total_len += logv->len;
		addr += logv->len;
		logv++;
	}

	ctx->persist_fn(start, total_len);

	log_header_update(ctx, total_len);

	return (int)logvcnt + 1;
}

/*
 * log_dump -- dump the LOG file content
 */
static void
log_dump(struct log_ctx *ctx)
{
	struct log *log = ctx->log;
	fwrite(log->data, sizeof(log->data[0]), log->header.used, stdout);
	printf("\n");
}

/*
 * parse_log_vec -- allocate and fill log_vec structure
 */
static int
parse_log_vec(struct log_vec *vec, uint8_t *addr, size_t log_vcnt)
{
	for (size_t i = 0; i < log_vcnt; ++i) {
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
		"\tlog <file> <COMMAND_1> [COMMAND_2 ...]\n"
		"\tlog help\n"
		"Available commands:\n"
		"append DATA\t\t\t- add a new element to the LOG\n"
		"appendv N DATA_1 ... DATA_N\t- add N new elements to the LOG\n"
		"rewind\t\t\t\t- remove the LOG content\n"
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

	struct log_ctx ctx;

	int ret = log_init(&ctx, argv[1]);
	if (ret)
		return ret;

	/* skip executable name and the LOG file name */
	argc -= 2;
	argv += 2;
	while (argc > 0) {
		if (strcmp(argv[0], "append") == 0 && argc > 1) {
			uint8_t *data = (uint8_t *)argv[1];
			size_t data_len = strlen((char *)data);

			ret = log_append(&ctx, data, data_len);
		} else if (strcmp(argv[0], "appendv") == 0 && argc > 1) {
			errno = 0;
			char *end;
			size_t log_vcnt = strtoul(argv[1], &end, 0);
			if (log_vcnt == 0 || (errno != 0 && *end == '\0')) {
				fprintf(stderr,
					"log_appendv: invalid N argument: %s\n",
					argv[1]);
				ret = 1;
				goto err_out;
			}
			/* number of needed arguments: 2 + log_vcnt */
			if (argc < 2 + log_vcnt) {
				fprintf(stderr,
					"log_appendv: a too small number of strings provided\n");
				ret = 1;
				goto err_out;
			}
			struct log_vec *vec;
			vec = malloc(sizeof(struct log_vec) * log_vcnt);
			if (!vec) {
				perror("malloc");
				ret = 1;
				goto err_out;
			}
			uint8_t *addr = (uint8_t *)argv[2];
			ret = parse_log_vec(vec, addr, log_vcnt);
			if (ret) {
				free(vec);
				goto err_out;
			}

			ret = log_appendv(&ctx, vec, log_vcnt);

			free(vec);
		} else if (strcmp(argv[0], "dump") == 0) {
			log_dump(&ctx);
		} else if (strcmp(argv[0], "rewind") == 0) {
			log_rewind(&ctx);
		} else {
			fprintf(stderr,
				"log: %s - unknown command or a too small number of arguments\n",
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

	ret = log_fini(&ctx);

	return ret;

err_out:
	(void) log_fini(&ctx);

	return ret;
}
