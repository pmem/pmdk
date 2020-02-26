// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2019, Intel Corporation */

/*
 * asset_list -- list all assets in an assetdb file
 *
 * Usage:
 *	asset_list /path/to/pm-aware/file
 */

#include <ex_common.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <libpmemblk.h>

#include "asset.h"

int
main(int argc, char *argv[])
{
	PMEMblkpool *pbp;
	size_t nelements;
	struct asset asset;

	if (argc < 2) {
		fprintf(stderr, "usage: %s assetdb\n", argv[0]);
		exit(1);
	}

	const char *path = argv[1];

	/* open an array of atomically writable elements */
	if ((pbp = pmemblk_open(path, sizeof(struct asset))) == NULL) {
		perror(path);
		exit(1);
	}

	/* how many elements do we have? */
	nelements = pmemblk_nblock(pbp);

	/* print out all the elements that contain assets data */
	for (size_t assetid = 0; assetid < nelements; ++assetid) {
		if (pmemblk_read(pbp, &asset, assetid) < 0) {
			perror("pmemblk_read");
			exit(1);
		}

		if ((asset.state != ASSET_FREE) &&
			(asset.state != ASSET_CHECKED_OUT)) {
			break;
		}

		printf("Asset ID: %zu\n", assetid);
		if (asset.state == ASSET_FREE)
			printf("   State: Free\n");
		else {
			printf("   State: Checked out\n");
			printf("    User: %s\n", asset.user);
			printf("    Time: %s", ctime(&asset.time));
		}
		printf("    Name: %s\n", asset.name);
	}

	pmemblk_close(pbp);
	return 0;
}
