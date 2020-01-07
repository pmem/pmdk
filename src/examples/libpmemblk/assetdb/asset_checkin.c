// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2019, Intel Corporation */

/*
 * asset_checkin -- mark an asset as no longer checked out
 *
 * Usage:
 *	asset_checkin /path/to/pm-aware/file asset-ID
 */

#include <ex_common.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <libpmemblk.h>

#include "asset.h"

int
main(int argc, char *argv[])
{
	PMEMblkpool *pbp;
	struct asset asset;
	int assetid;

	if (argc < 3) {
		fprintf(stderr, "usage: %s assetdb asset-ID\n", argv[0]);
		exit(1);
	}

	const char *path = argv[1];
	assetid = atoi(argv[2]);
	assert(assetid > 0);

	/* open an array of atomically writable elements */
	if ((pbp = pmemblk_open(path, sizeof(struct asset))) == NULL) {
		perror("pmemblk_open");
		exit(1);
	}

	/* read a required element in */
	if (pmemblk_read(pbp, &asset, assetid) < 0) {
		perror("pmemblk_read");
		exit(1);
	}

	/* check if it contains any data */
	if ((asset.state != ASSET_FREE) &&
		(asset.state != ASSET_CHECKED_OUT)) {
		fprintf(stderr, "Asset ID %d not found\n", assetid);
		exit(1);
	}

	/* change state to free, clear user name and timestamp */
	asset.state = ASSET_FREE;
	asset.user[0] = '\0';
	asset.time = 0;

	if (pmemblk_write(pbp, &asset, assetid) < 0) {
		perror("pmemblk_write");
		exit(1);
	}

	pmemblk_close(pbp);
	return 0;
}
