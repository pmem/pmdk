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
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "libpmem2.h"
#include "file.h"
#include "os.h"
#include "util.h"

#define KILOBYTE (1 << 10)
#define NOT_SET_FD (-1)

enum gran_detecto_mode {
	NOT_SET,
	VALIDATE,
	DETECT,
};

/*
 * tool_ctx -- essential parameters used by gran_detecto
 */
struct tool_ctx
{
	enum pmem2_granularity actual_granularity;
	char *probe_file_path;
	int fd;
	enum gran_detecto_mode state;

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
	"-d, --detect        - detect the smallest available granularity for <path>\n"
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
	/* tool context initialization */
	ctx->expected_granularity = PMEM2_GRANULARITY_PAGE;
	ctx->actual_granularity = PMEM2_GRANULARITY_PAGE;
	ctx->fd = NOT_SET_FD;
	ctx->state = NOT_SET;
	ctx->probe_file_path = NULL;
	ctx->path = NULL;

	int opt;
	while ((opt = getopt_long(argc, argv, "bcdhp", long_options, NULL)) !=
		-1) {
		switch (opt) {
			case 'b':
				ctx->state = VALIDATE;
				ctx->expected_granularity =
							PMEM2_GRANULARITY_BYTE;
				break;
			case 'c':
				ctx->state = VALIDATE;
				ctx->expected_granularity =
						PMEM2_GRANULARITY_CACHE_LINE;
				break;
			case 'd':
				ctx->state = DETECT;
				break;
			case 'p':
				ctx->state = VALIDATE;
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

	if (ctx->state == NOT_SET) {
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
	assert(ctx->fd != NOT_SET_FD);
	os_close(ctx->fd);
	ctx->fd = NOT_SET_FD;
}
#else
static void
cleanup_file(struct tool_ctx *ctx)
{
	if (ctx->fd != NOT_SET_FD) {
		os_close(ctx->fd);
		ctx->fd = NOT_SET_FD;
		os_unlink(ctx->probe_file_path);
	}

	free(ctx->probe_file_path);
}
#endif

/*
 * prepare_file -- create, prepare a file
 */
#ifdef __linux__
static void
prepare_file(struct tool_ctx *ctx)
{
	ctx->fd = os_open(ctx->path, O_TMPFILE | O_RDWR, 0640);
	if (ctx->fd < 0) {
		perror("os_open failed");
		return;
	}

	int ret = os_ftruncate(ctx->fd, 16 * KILOBYTE);
	if (ret) {
		perror("os_ftruncate failed");
		goto cleanup_file;
	}

	return;

cleanup_file:
	cleanup_file(ctx);
}
#else
static void
prepare_file(struct tool_ctx *ctx)
{
	ctx->probe_file_path = malloc(PATH_MAX * sizeof(char));
	if (!ctx->probe_file_path) {
		perror("malloc failed");
		return;
	}

	int ret = snprintf(ctx->probe_file_path, PATH_MAX,
			"%s" OS_DIR_SEP_STR "temp_grandetecto", ctx->path);
	if (ret < 0) {
		perror("snprintf failed");
		goto cleanup_file;
	} else if (ret >= PATH_MAX) {
		fprintf(stderr,
			"snprintf failed: number of characters exceeds PATH_MAX.\n");
		goto cleanup_file;
	}

	ctx->fd = os_open(ctx->probe_file_path, O_CREAT | O_RDWR, 0640);
	if (ctx->fd < 0) {
		perror("os_open failed");
		ctx->fd = NOT_SET_FD;
		goto cleanup_file;
	}

	const char *message =
			"This file was created by gran_detecto. It can be safely removed.";
	ret = util_write(ctx->fd, message, strlen(message));
	if (ret == -1) {
		perror("util_write failed");
		goto cleanup_file;
	}

	return;

cleanup_file:
	cleanup_file(ctx);
}
#endif

/*
 * gran_detecto -- try to map file and get the smallest available granularity
 */
static int
gran_detecto(struct tool_ctx *ctx)
{
	int ret = 0;
	prepare_file(ctx);
	if (ctx->fd == NOT_SET_FD)
		return 1;

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
#ifdef _WIN32
	util_suppress_errmsg();
	wchar_t **wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
	for (int i = 0; i < argc; i++) {
		argv[i] = util_toUTF8(wargv[i]);
		if (argv[i] == NULL) {
			for (i--; i >= 0; i--)
				free(argv[i]);
			fprintf(stderr,
				"gran_detecto: error during arguments conversion\n");
			return 1;
		}
	}
#endif
	struct tool_ctx ctx = {0};
	int ret = 0;
	if (parse_args(argc, argv, &ctx)) {
		ret = 1;
		goto out;
	}

	if (gran_detecto(&ctx)) {
		ret = 1;
		goto out;
	}

	if (ctx.state == DETECT) {
		printf(
			"gran_detecto: the smallest available granularity for %s is %s\n",
			ctx.path,
			PMEM2_GRANULARITIES[ctx.actual_granularity]);
		goto out;
	}

	ret = ctx.expected_granularity == ctx.actual_granularity ? 0 : 1;

out:
#ifdef _WIN32
	for (int i = argc; i > 0; i--)
		free(argv[i - 1]);
#endif

	return ret;
}
