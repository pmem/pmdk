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
 * grandetecto.c -- detect required store/flush granularity
 */
#define _GNU_SOURCE

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

/*
 * print_usage -- print short description of usage
 */
static void
print_usage(void)
{
	printf("Usage: grandetecto <-b|-c|-p|-h> <path>\n"
	"Valid options:\n"
	"-b, --byte          - check if <path> has byte granularity\n"
	"-c, --cache-line    - check if <path> has cache line "
	"granularity\n"
	"-p, --page          - check if <path> has page granularity\n"
	"-d, --directory=DIR - path to the directory whose granularity will"
	"be detected\n"
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
 * check_granularity -- try to map file with given granularity
 */
static int
check_granularity(enum pmem2_granularity granularity, struct pmem2_config *cfg)
{
	struct pmem2_map *map;
	int ret = 0;
	if (pmem2_map(cfg, &map)) {
		printf("pmem2_map failed");
		return -1;
	}
	if (pmem2_map_get_store_granularity(map) != granularity)
		ret = -1;

	if (pmem2_unmap(&map))
		printf("pmem2_unmap failed");

	return ret;
}

int
main(int argc, char *argv[])
{
	if (argc > 3) {
		printf("grandetecto: too many arguments\n");
		print_usage();
		return -1;
	}

	char *path = NULL;
	int scenario = 0;
	int ret = 0;
	int opt;
	while ((opt = getopt_long(argc, argv, "bchp", long_options, NULL)) !=
		-1) {
		switch (opt) {
			case 'b':
				scenario = 1;
				break;
			case 'c':
				scenario = 2;
				break;
			case 'p':
				scenario = 3;
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
		printf("grandetecto: path cannot be empty.\n");
		print_usage();
		return -1;
	}

	/* fill config in minimal scope */
	struct pmem2_config *cfg;
	pmem2_config_new(&cfg);

#ifdef __linux__
	int fd = open(path, O_TMPFILE | O_RDWR, 0640);
#else
	char *file = malloc(strlen(path));
	strcpy(file, path);
	strcat(file, "temp_grandetecto");
	int fd = os_open(file, O_CREAT | O_RDWR, 0640);
#endif

	if (fd < 0) {
		ret = -1;
		printf("os_open failed");
		goto free_config;
	}
	ret = os_ftruncate(fd, 16 * KILOBYTE);
	if (ret)
		goto close_fd;

#ifndef __linux__
	char *message = "This is a file used by grandetecto. If grandetecto is"
	" inactive, you can remove this file.";
	ret = write(fd, message, (int)strlen(message));
	if (ret == -1)
		goto close_fd;
#endif

	if (pmem2_config_set_fd(cfg, fd)) {
		printf("pmem2_config_set_fd failed");
		ret = -1;
		goto close_fd;
	}

	switch (scenario) {
		case 1:
			ret = check_granularity(PMEM2_GRANULARITY_BYTE,
						cfg);
			break;
		case 2:
			ret = check_granularity(
				PMEM2_GRANULARITY_CACHE_LINE, cfg);
			break;
		case 3:
			check_granularity(PMEM2_GRANULARITY_PAGE,
						cfg);
			break;
		default:
			break;
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
