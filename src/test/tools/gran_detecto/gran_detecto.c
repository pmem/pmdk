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
 * gran_detecto.c -- detect available store/flush granularity
 */
#define _GNU_SOURCE

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libpmem2.h"
#include "os.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define GRANULARITY_DETECT 99
#define KILOBYTE (1 << 10)
#define NOT_SET (-1)

/*
 * tool_ctx -- essential parameters used by gran_detecto
 */
struct tool_ctx
{
	enum pmem2_granularity actual_granularity;
	char *file;
	int fd;
	/* user arguments */
	enum pmem2_granularity expected_granularity;
	char *path;
};

/*
 * print_usage -- print short description of usage
 */
static void
print_usage(void)
{
	printf("Usage: gran_detecto -h\n"
	"       gran_detecto <-b|-c|-d|-p> <path>\n"
	"Available options:\n"
	"-b, --byte          - check if <path> has byte granularity\n"
	"-c, --cache-line    - check if <path> has cache line granularity\n"
	"-d, --detect        - detect the smallest available granularity for <path>"
	"-p, --page          - check if <path> has page granularity\n"
	"-h, --help          - print this usage info\n");
}

/*
 * long_options -- command line options
 */
static const struct option long_options[] = {
	{"byte", no_argument, NULL, 'b'},
	{"cache-line", no_argument, NULL, 'c'},
	{"detect", no_argument, NULL, 'd'},
	{"page", no_argument, NULL, 'p'},
	{"help", no_argument, NULL, 'h'},
	{NULL, 0, NULL, 0},
};

/*
 * parse_args -- parse command line arguments
 */
static int
parse_args(int argc, char *argv[], struct tool_ctx *ctx)
{
	if (argc > 3) {
		fprintf(stderr, "gran_detecto: too many arguments\n");
		print_usage();
		return 1;
	}

	/* tool context initialization */
	ctx->expected_granularity = (enum pmem2_granularity)NOT_SET;
	ctx->actual_granularity = (enum pmem2_granularity)NOT_SET;
	ctx->fd = NOT_SET;
	ctx->file = NULL;
	ctx->path = NULL;

	int opt;
	while ((opt = getopt_long(argc, argv, "bcdhp", long_options, NULL)) !=
		-1) {
		switch (opt) {
			case 'b':
				ctx->expected_granularity =
							PMEM2_GRANULARITY_BYTE;
				break;
			case 'c':
				ctx->expected_granularity =
						PMEM2_GRANULARITY_CACHE_LINE;
				break;
			case 'd':
				ctx->expected_granularity =
							GRANULARITY_DETECT;
				break;
			case 'p':
				ctx->expected_granularity =
							PMEM2_GRANULARITY_PAGE;
				break;
			case 'h':
				print_usage();
				exit(0);
			default:
				print_usage();
				return 1;
		}
	}

	if ((int)ctx->expected_granularity == NOT_SET) {
		fprintf(stderr, "gran_detecto: required argument missing.\n");
		print_usage();
		return 1;
	}

	if (optind < argc) {
		ctx->path = argv[optind];
	} else {
		fprintf(stderr, "gran_detecto: path cannot be empty.\n");
		print_usage();
		return 1;
	}

	return 0;
}

/*
 * cleanup_file -- close the file descriptor and remove the file and free file
 * name variable if applicable
 */
#ifdef __linux__
static void
cleanup_file(struct tool_ctx *ctx)
{
	assert(ctx->fd != NOT_SET);
	close(ctx->fd);
	ctx->fd = NOT_SET;
}
#else
static void
cleanup_file(struct tool_ctx *ctx)
{
	if (ctx->fd != NOT_SET) {
		close(ctx->fd);
		ctx->fd = NOT_SET;
		os_unlink(ctx->file);
	}

	free(ctx->file);
}
#endif

/*
 * prepare_file -- create, prepare a file
 */
#ifdef __linux__
static void
prepare_file(struct tool_ctx *ctx)
{
	ctx->fd = open(ctx->path, O_TMPFILE | O_RDWR, 0640);
	if (ctx->fd < 0) {
		fprintf(stderr, "open failed: %s\n", strerror(errno));
		return;
	}

	int ret = 0;
	ret = os_ftruncate(ctx->fd, 16 * KILOBYTE);
	if (ret)
		goto cleanup_file;

	return;

cleanup_file:
	cleanup_file(ctx);
}
#else
static void
prepare_file(struct tool_ctx *ctx)
{
	ctx->file = malloc(PATH_MAX * sizeof(char));
	if (!ctx->file) {
		fprintf(stderr, "malloc failed\n");
		return;
	}

	int ret = 0;
	ret = sprintf(ctx->file, "%s" OS_DIR_SEP_STR "temp_grandetecto",
			ctx->path);
	if (ret < 0) {
		fprintf(stderr, "sprintf failed\n");
		goto cleanup_file;
	}

	ctx->fd = os_open(ctx->file, O_CREAT | O_RDWR, 0640);
	if (ctx->fd < 0) {
		fprintf(stderr, "open failed: %s\n", strerror(errno));
		ctx->fd = NOT_SET;
		goto cleanup_file;
	}

	ret = os_ftruncate(ctx->fd, 16 * KILOBYTE);
	if (ret) {
		fprintf(stderr, "os_ftruncate failed: %s\n", strerror(errno));
		goto cleanup_file;
	}

	const char *message =
			"This file was created by gran_detecto. It can be safely removed.";
	ret = write(ctx->fd, message, (int)strlen(message));
	if (ret == -1) {
		fprintf(stderr, "write failed: %s\n", strerror(errno));
		goto cleanup_file;
	}

	return;

cleanup_file:
	cleanup_file(ctx);
}
#endif

/*
 * gran_detecto -- try to map file and get the smallest available
 * granularity
 */
static int
gran_detecto(struct tool_ctx *ctx)
{
	int ret = 0;
	prepare_file(ctx);
	if (ctx->fd == NOT_SET) {
		return 1;
	}

	/* fill config in minimal scope */
	struct pmem2_config *cfg;
	if (pmem2_config_new(&cfg)) {
		fprintf(stderr, "pmem2_config_new failed: %s\n",
			pmem2_errormsg());
		ret = 1;
		goto cleanup_file;
	}

	if (pmem2_config_set_fd(cfg, ctx->fd)) {
		fprintf(stderr, "pmem2_config_set_fd failed: %s\n",
			pmem2_errormsg());
		ret = 1;
		goto free_config;
	}

	struct pmem2_map *map;
	if (pmem2_map(cfg, &map)) {
		fprintf(stderr, "pmem2_map failed: %s\n", pmem2_errormsg());
		ret = 1;
		goto free_config;
	}

	ctx->actual_granularity = pmem2_map_get_store_granularity(map);

	if (pmem2_unmap(&map)) {
		fprintf(stderr, "pmem2_unmap failed: %s\n", pmem2_errormsg());
		ret = 1;
	}

free_config:
	pmem2_config_delete(&cfg);
cleanup_file:
	cleanup_file(ctx);

	return ret;
}

static const char *PMEM2_GRANULARITIES[] = {"",
					    "byte granularity",
					    "cache line granularity",
					    "page granularity"};

int
main(int argc, char *argv[])
{
	struct tool_ctx ctx = {0};
	if (parse_args(argc, argv, &ctx))
		return 1;

	if (gran_detecto(&ctx))
		return 1;

	if (ctx.expected_granularity == GRANULARITY_DETECT) {
		printf(
			"gran_detecto: the smallest avaiable granularity for %s is %s\n",
			ctx.path,
			PMEM2_GRANULARITIES[ctx.actual_granularity]);
		return 0;
	}

	return ctx.expected_granularity == ctx.actual_granularity ? 0 : 1;
}
