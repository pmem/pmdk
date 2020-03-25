// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2020, Intel Corporation */

/*
 * extents -- extents listing
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "os.h"
#include "extent.h"

#define B2SEC(n) ((n) >> 9)	/* convert bytes to sectors */

enum modes {
	MODE_PRINT_ALL_EXTENTS = 0,
	MODE_PRINT_ONE_PHY_OF_LOG,
};

static const char *usage_str =
	"usage: %s "
	"[-h] "
	"[-l <logical_offset>] "
	"<file>\n";

int
main(int argc, char *argv[])
{
	long unsigned offset = 0;
	unsigned extent = 0;
	char *error;
	int ret = -1;
	int opt;

	enum modes mode = MODE_PRINT_ALL_EXTENTS;

	while ((opt = getopt(argc, argv, "hl:")) != -1) {
		switch (opt) {
		case 'h':
			printf(usage_str, argv[0]);
			return 0;

		case 'l':
			mode = MODE_PRINT_ONE_PHY_OF_LOG;
			errno = 0;
			offset = strtoul(optarg, &error, 10 /* base */);
			if (errno || *error != '\0') {
				if (errno)
					perror("strtoul");
				if (*error != '\0') {
					fprintf(stderr,
						"error: invalid character(s) in the given logical offset: %s\n",
						error);
				}
				return -1;
			}
			break;

		default:
			fprintf(stderr, usage_str, argv[0]);
			return -1;
		}
	}

	if (optind + 1 < argc) {
		fprintf(stderr, "error: unknown option: %s\n",
			argv[optind + 1]);
		fprintf(stderr, usage_str, argv[0]);
		return -1;
	}

	if (optind >= argc) {
		fprintf(stderr, usage_str, argv[0]);
		return -1;
	}

	const char *file = argv[optind];

	struct extents *exts = malloc(sizeof(struct extents));
	if (exts == NULL)
		return -1;

	int fd = os_open(file, O_RDONLY);
	if (fd == -1) {
		perror(file);
		goto exit_free;
	}

	long count = os_extents_count(fd, exts);
	if (count < 0)
		goto exit_free;

	if (count == 0) {
		ret = 0;
		goto exit_free;
	}

	exts->extents = malloc(exts->extents_count * sizeof(struct extent));
	if (exts->extents == NULL)
		goto exit_free;

	ret = os_extents_get(fd, exts);
	if (ret)
		goto exit_free;

	switch (mode) {
	case MODE_PRINT_ALL_EXTENTS:
		for (unsigned e = 0; e < exts->extents_count; e++) {
			/* extents are in bytes, convert them to sectors */
			printf("%lu %lu\n",
				B2SEC(exts->extents[e].offset_physical),
				B2SEC(exts->extents[e].length));
		}
		break;

	case MODE_PRINT_ONE_PHY_OF_LOG:
		/* print the physical offset of the given logical one */
		for (unsigned e = 0; e < exts->extents_count; e++) {
			if (B2SEC(exts->extents[e].offset_logical) > offset)
				break;
			extent = e;
		}

		if (extent == exts->extents_count - 1) {
			long unsigned max_log;

			max_log = B2SEC(exts->extents[extent].offset_logical) +
					B2SEC(exts->extents[extent].length);

			if (offset > max_log) {
				fprintf(stderr,
					"error: maximum logical offset is %lu\n",
					max_log);
				ret = -1;
				goto exit_free;
			}
		}

		offset += B2SEC(exts->extents[extent].offset_physical) -
				B2SEC(exts->extents[extent].offset_logical);

		printf("%lu\n", offset);
		break;

	default:
		fprintf(stderr, usage_str, argv[0]);
		return -1;
	}

exit_free:
	if (exts->extents)
		free(exts->extents);
	free(exts);

	if (fd != -1)
		close(fd);

	return ret;
}
