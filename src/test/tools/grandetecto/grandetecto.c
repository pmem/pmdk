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

#include <getopt.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <libpmem2.h>

/*
 * print_usage -- print short description of usage
 */
static void
print_usage(void)
{
	printf("Usage: grandetecto <-b|-c|-p|-h> <path>\n");
	printf("Valid options:\n");
	printf("-b, --byte	  - check if <path> has byte granularity\n");
	printf("-c, --cache-line  - check if <path> has cache line");
	printf("granularity\n");
	printf("-p, --page	  - check if <path> has page granularity\n");
	printf("-h, --help	  - print this usage info\n");
}

/*
 * long_options -- command line options
 */
static const struct option long_options[] = {
	{"byte", no_argument, NULL, 'b'},
	{"cache-line", required_argument, NULL, 'c'},
	{"page", no_argument, NULL, 'p'},
	{"help", no_argument, NULL, 'h'},
	{NULL, 0, NULL, 0},
};


/*
 * prepare_cfg -- fill pmem2_config in minimal scope
 */
static int
prepare_cfg(char *path, struct pmem2_config **cfg)
{
	pmem2_config_new(cfg);
	int fd = open(path, O_CREAT, 0640);
	if (fd < 0) {
		perror(path);
		return -1;
	}
	if (pmem2_config_set_fd(*cfg, fd))
		return -1;
	return 0;
}

/*
 * check_granularity -- try to map file with given granularity
 */
static int
check_granularity(enum pmem2_granularity granularity, struct pmem2_config *cfg)
{
	struct pmem2_map *map;
	pmem2_config_set_required_store_granularity(cfg, granularity);
	if (pmem2_map(cfg, &map))
		return -1;

	if (pmem2_unmap(&map))
		return -1;

	return 0;
}

int
main(int argc, char *argv[])
{
	char *file = argv[argc - 1];
	strcat(file, "/testfile");
	struct pmem2_config *cfg;
	if (prepare_cfg(file, &cfg))
		return -1;

	int opt;
	int ret = 0;
	while ((opt = getopt_long(argc, argv, "bchp", long_options, NULL)) !=
		-1) {
		switch (opt) {
			case 'b':
				ret = check_granularity(PMEM2_GRANULARITY_BYTE,
							cfg);
				if (ret)
					ret = -1;
				break;
			case 'c':
				ret = check_granularity(
					PMEM2_GRANULARITY_CACHE_LINE, cfg);
				if (ret)
					ret = -1;
				ret = check_granularity(PMEM2_GRANULARITY_BYTE,
							cfg);
				if (!ret)
					ret = -1;
				break;
			case 'p':
				check_granularity(PMEM2_GRANULARITY_CACHE_LINE,
							cfg);
				if (!ret)
					ret = -1;
				break;
			case 'h':
				print_usage();
			default:
				print_usage();
				ret = -1;
		}
	}

	unlink(file);
	return ret;
}
