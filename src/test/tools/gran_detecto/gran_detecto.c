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
 * gran_detecto.c -- detect required store/flush granularity
 */
#define _GNU_SOURCE

#include <errno.h>
#include <getopt.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#include "libpmem2.h"
#include "os.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define KILOBYTE (1 << 10)
#define GRANULARITY_NO_CHECK 99

/*
 * tool_ctx -- essential parameters used by gran_detecto
 */
struct tool_ctx
{
	enum pmem2_granularity actual_granularity;
	/* user arguments */
	char *path;
	enum pmem2_granularity expected_granularity;
};

/*
 * print_usage -- (internal) print short description of usage
 */
static void
print_usage(void)
{
	printf("Usage: gran_detecto <-b|-c|-d|-p|-h> <path>\n"
	"Valid options:\n"
	"-b, --byte          - check if <path> has byte granularity\n"
	"-c, --cache-line    - check if <path> has cache line granularity\n"
	"-d, --default       - detect the smallest current granularity for <path>"
	"-p, --page          - check if <path> has page granularity\n"
	"-h, --help          - print this usage info\n");
}

/*
 * long_options -- command line options
 */
static const struct option long_options[] = {
	{"byte", no_argument, NULL, 'b'},
	{"cache-line", no_argument, NULL, 'c'},
	{"default", no_argument, NULL, 'd'},
	{"page", no_argument, NULL, 'p'},
	{"help", no_argument, NULL, 'h'},
	{NULL, 0, NULL, 0},
};

/*
 * parse_args -- (internal) parse command line arguments
 */
static int
parse_args(int argc, char *argv[], struct tool_ctx *ctx)
{
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
							GRANULARITY_NO_CHECK;
				break;
			case 'p':
				ctx->expected_granularity =
							PMEM2_GRANULARITY_PAGE;
				break;
			case 'h':
				print_usage();
				return 0;
			default:
				print_usage();
				return 1;
		}
	}

	if ((int)ctx->expected_granularity == -1) {
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
 * prepare_file -- (internal) create, prepare a file and return
 * file descriptor
 */
#ifdef __linux__
static int
prepare_file(struct tool_ctx ctx)
{
	int fd = open(ctx.path, O_TMPFILE | O_RDWR, 0640);
	if (fd < 0) {
		fprintf(stderr, "open failed: %s\n", strerror(errno));
		return -1;
	}

	int ret = 0;
	ret = os_ftruncate(fd, 16 * KILOBYTE);
	if (ret)
		goto close_fd;

	return fd;

close_fd:
	close(fd);

	return -1;
}
#else
static int
prepare_file(struct tool_ctx ctx, char **file)
{
	size_t length = strlen(ctx.path) + strlen("temp_grandetecto") + 2;
	*file = malloc(length * sizeof(char));
	sprintf(*file, "%s"OS_DIR_SEP_STR"temp_grandetecto", ctx.path);

	int fd = os_open(*file, O_CREAT | O_RDWR, 0640);
	if (fd < 0) {
		fprintf(stderr, "open failed: %s\n", strerror(errno));
		goto free_file;
	}

	int ret = 0;
	ret = os_ftruncate(fd, 16 * KILOBYTE);
	if (ret)
		goto close_fd;

	const char *message =
			"This file was created by gran_detecto. It can be safely removed.";
	ret = write(fd, message, (int)strlen(message));
	if (ret == -1)
		goto close_fd;

	return fd;

close_fd:
	close(fd);
	os_unlink(*file);
free_file:
	free(*file);
	return -1;
}
#endif

/*
 * check_granularity -- (internal) try to map file with given granularity and
 * set the smallest available granularity
 */
static int
check_granularity(enum pmem2_granularity granularity, struct tool_ctx *ctx)
{
	int ret = 0;
	/* fill config in minimal scope */
	struct pmem2_config *cfg;
	if (pmem2_config_new(&cfg)) {
		fprintf(stderr, "pmem2_config_new failed: %s\n",
			pmem2_errormsg());
		return 1;
	}

#ifndef __linux__
	static char *file;
	int fd = prepare_file(*ctx, &file);
#else
	int fd = prepare_file(*ctx);
#endif
	if (fd == -1) {
		ret = 1;
		goto free_config;
	}

	if (pmem2_config_set_fd(cfg, fd)) {
		fprintf(stderr, "pmem2_config_set_fd failed: %s\n",
			pmem2_errormsg());
		ret = 1;
		goto close_fd;
	}

	struct pmem2_map *map;
	if (pmem2_map(cfg, &map)) {
		fprintf(stderr, "pmem2_map failed: %s\n", pmem2_errormsg());
		ret = 1;
		goto close_fd;
	}

	ctx->actual_granularity = pmem2_map_get_store_granularity(map);
	if (ctx->actual_granularity != granularity)
		ret = 1;

	if (pmem2_unmap(&map)) {
		fprintf(stderr, "pmem2_unmap failed: %s\n", pmem2_errormsg());
		ret = 1;
	}

close_fd:
	close(fd);
#ifndef __linux__
	os_unlink(file);
	free(file);
#endif
free_config:
	pmem2_config_delete(&cfg);

	return ret;
}

static const char *PMEM2_GRANULARITIES[] = {"",
					    "byte granularity",
					    "cache line granularity",
					    "page granularity"};

int
main(int argc, char *argv[])
{
	if (argc > 3) {
		fprintf(stderr, "gran_detecto: too many arguments\n");
		print_usage();
		return 1;
	}

	int ret = 0;
	struct tool_ctx ctx;
	ctx.expected_granularity = -1;
	if (parse_args(argc, argv, &ctx))
		return 1;

	switch ((int)ctx.expected_granularity) {
		case PMEM2_GRANULARITY_BYTE:
			ret = check_granularity(PMEM2_GRANULARITY_BYTE,
						&ctx);
			break;
		case PMEM2_GRANULARITY_CACHE_LINE:
			ret = check_granularity(PMEM2_GRANULARITY_CACHE_LINE,
						&ctx);
			break;
		case PMEM2_GRANULARITY_PAGE:
			ret = check_granularity(PMEM2_GRANULARITY_PAGE,
						&ctx);
			break;
		case GRANULARITY_NO_CHECK:
			check_granularity(GRANULARITY_NO_CHECK, &ctx);
			printf(
				"gran_detecto: the smallest avaiable granularity for %s is %s\n",
				ctx.path,
				PMEM2_GRANULARITIES[ctx.actual_granularity]);
			break;
		default:
			return 1;
	}

	return ret;
}
