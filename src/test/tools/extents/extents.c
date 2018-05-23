/*
 * Copyright 2018, Intel Corporation
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
 * extents -- extents listing
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

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

	long count = os_extents_count(file, exts);
	if (count < 0)
		goto exit_free;

	if (count == 0) {
		ret = 0;
		goto exit_free;
	}

	exts->extents = malloc(exts->extents_count * sizeof(struct extent));
	if (exts->extents == NULL)
		goto exit_free;

	ret = os_extents_get(file, exts);
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

	return ret;
}
