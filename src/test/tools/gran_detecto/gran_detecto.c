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
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "os.h"

#include <libpmem2.h>

#define KILOBYTE (1 << 10)

enum granularity_type {
	BYTE,
	CACHE_LINE,
	PAGE
};

/* arguments */
static char *path;
static enum granularity_type type;
/*
 * print_usage -- print short description of usage
 */
static void
print_usage(void)
{
	printf("Usage: gran_detecto <-b|-c|-p|-h> <path>\n"
	"Valid options:\n"
	"-b, --byte          - check if <path> has byte granularity\n"
	"-c, --cache-line    - check if <path> has cache line granularity\n"
	"-p, --page          - check if <path> has page granularity\n"
	"-d, --directory=DIR - path to the directory whose granularity will be detected\n"
	"-h, --help          - print this usage info\n");
}

/*
 * long_options -- command line options
 */
static const struct option long_options[] = {
	{"byte", no_argument, NULL, 'b'},
	{"cache-line", no_argument, NULL, 'c'},
	{"page", no_argument, NULL, 'p'},
	{"help", no_argument, NULL, 'h'},
	{NULL, 0, NULL, 0},
};

/*
 * parse_args -- (internal) parse command line arguments
 */
static int
parse_args(int argc, char *argv[])
{
	int opt;
	while ((opt = getopt_long(argc, argv, "bchp", long_options, NULL)) !=
		-1) {
		switch (opt) {
			case 'b':
				type = BYTE;
				break;
			case 'c':
				type = CACHE_LINE;
				break;
			case 'p':
				type = PAGE;
				break;
			case 'h':
				print_usage();
				return 0;
			default:
				print_usage();
				return -1;
		}
	}

	if (optind < argc) {
		path = argv[optind];
	} else {
		fprintf(stderr, "gran_detecto: path cannot be empty.\n");
		print_usage();
		return -1;
	}

	return 0;
}

/*
 * prepare_file -- create and prepare a file
 */
static int
prepare_file(void)
{
	int ret = 0;
#ifdef __linux__
	int fd = open(path, O_TMPFILE | O_RDWR, 0640);
#else
	char *suffix = "/temp_grandetecto";
	size_t length = strlen(path) + strlen(suffix) + 1;
	char *file = malloc(length);
	strcpy(file, path);
	strcat(file, suffix);
	int fd = os_open(file, O_CREAT | O_RDWR, 0640);
#endif

	if (fd < 0) {
		fprintf(stderr, "open failed: %s", strerror(errno));
		return -1;
	}
	ret = os_ftruncate(fd, 16 * KILOBYTE);
	if (ret)
		goto close_fd;

#ifndef __linux__
	char *message = "This file was created by gran_detecto. It can be"
	" safely removed.";
	ret = write(fd, message, (int)strlen(message));
	if (ret == -1)
		goto close_fd;
#endif

	return fd;

close_fd:
	close(fd);
#ifndef __linux__
	os_unlink(file);
#endif
	return -1;
}

/*
 * check_granularity -- try to map file with given granularity
 */
static int
check_granularity(enum pmem2_granularity granularity)
{
	int ret = 0;
	/* fill config in minimal scope */
	struct pmem2_config *cfg;
	pmem2_config_new(&cfg);

	int fd = prepare_file();
	if (fd == -1) {
		ret = -1;
		goto free_config;
	}

	if (pmem2_config_set_fd(cfg, fd)) {
		fprintf(stderr, "pmem2_config_set_fd failed");
		ret = -1;
		goto close_fd;
	}

	struct pmem2_map *map;
	if (pmem2_map(cfg, &map)) {
		fprintf(stderr, "pmem2_map failed");
		ret = -1;
		goto close_fd;
	}

	if (pmem2_map_get_store_granularity(map) != granularity)
		ret = -1;

	if (pmem2_unmap(&map)) {
		fprintf(stderr, "pmem2_unmap failed");
		ret = -1;
	}

close_fd:
	close(fd);
#ifndef __linux__
	os_unlink(file);
#endif
free_config:
	pmem2_config_delete(&cfg);

	return ret;
}

int
main(int argc, char *argv[])
{
	if (argc > 3) {
		fprintf(stderr, "gran_detecto: too many arguments\n");
		print_usage();
		return -1;
	}

	int ret = 0;

	if (parse_args(argc, argv))
		return -1;

	switch (type) {
		case BYTE:
			ret = check_granularity(PMEM2_GRANULARITY_BYTE);
			break;
		case CACHE_LINE:
			ret = check_granularity(PMEM2_GRANULARITY_CACHE_LINE);
			break;
		case PAGE:
			ret = check_granularity(PMEM2_GRANULARITY_PAGE);
			break;
		default:
			break;
	}

	return ret;
}
