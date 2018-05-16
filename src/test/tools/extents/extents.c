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

#include "extent.h"

#define B2SEC(n) ((n) >> 9)	/* convert bytes to sectors */

int
main(int argc, char *argv[])
{
	int ret = -1;

	if (argc < 2 || argc > 3) {
		fprintf(stderr, "usage: %s file [logical-block-number]\n",
				argv[0]);
		return -1;
	}

	struct extents *exts = malloc(sizeof(struct extents));
	if (exts == NULL)
		return -1;

	long count = os_extents_count(argv[1], exts);
	if (count < 0)
		goto exit_free;

	if (count == 0) {
		ret = 0;
		goto exit_free;
	}

	exts->extents = malloc(exts->extents_count * sizeof(struct extent));
	if (exts->extents == NULL)
		goto exit_free;

	ret = os_extents_get(argv[1], exts);
	if (ret)
		goto exit_free;

	if (argc == 2) {
		/* print out all extents */
		for (unsigned e = 0; e < exts->extents_count; e++) {
			/* extents are in bytes, convert them to sectors */
			printf("%lu %lu\n",
				B2SEC(exts->extents[e].offset_physical),
				B2SEC(exts->extents[e].length));
		}
	} else {
		/* print the physical block of a given logical one */
		long unsigned block = (long unsigned)atol(argv[2]);

		long unsigned last_phy =
					B2SEC(exts->extents[0].offset_physical);
		long unsigned last_log = B2SEC(exts->extents[0].offset_logical);

		for (unsigned e = 0; e < exts->extents_count; e++) {
			if (B2SEC(exts->extents[e].offset_logical) > block)
				break;
			last_log = B2SEC(exts->extents[e].offset_logical);
			last_phy = B2SEC(exts->extents[e].offset_physical);
		}

		block += last_phy - last_log;

		printf("%lu\n", block);
	}

exit_free:
	if (exts->extents)
		free(exts->extents);

	free(exts);

	return ret;
}
