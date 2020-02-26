// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2019, Intel Corporation */

/*
 * asset_load -- given pre-allocated assetdb file, load it up with assets
 *
 * Usage:
 *	fallocate -l 1G /path/to/pm-aware/file
 *	asset_load /path/to/pm-aware/file asset-file
 *
 * The asset-file should contain the names of the assets, one per line.
 */

#include <ex_common.h>
#include <sys/stat.h>
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
	FILE *fp;
	int len = ASSET_NAME_MAX;
	PMEMblkpool *pbp;
	size_t assetid = 0;
	size_t nelements;
	char *line;

	if (argc < 3) {
		fprintf(stderr, "usage: %s assetdb assetlist\n", argv[0]);
		exit(1);
	}

	const char *path_pool = argv[1];
	const char *path_list = argv[2];

	/* create pmemblk pool in existing (but as yet unmodified) file */
	pbp = pmemblk_create(path_pool, sizeof(struct asset),
			0, CREATE_MODE_RW);

	if (pbp == NULL) {
		perror(path_pool);
		exit(1);
	}

	nelements = pmemblk_nblock(pbp);

	if ((fp = fopen(path_list, "r")) == NULL) {
		perror(path_list);
		exit(1);
	}

	/*
	 * Read in all the assets from the assetfile and put them in the
	 * array, if a name of the asset is longer than ASSET_NAME_SIZE_MAX,
	 * truncate it.
	 */
	line = malloc(len);
	if (line == NULL) {
		perror("malloc");
		exit(1);
	}
	while (fgets(line, len, fp) != NULL) {
		struct asset asset;

		if (assetid >= nelements) {
			fprintf(stderr, "%s: too many assets to fit in %s "
					"(only %zu assets loaded)\n",
					path_list, path_pool, assetid);
			exit(1);
		}

		memset(&asset, '\0', sizeof(asset));
		asset.state = ASSET_FREE;
		strncpy(asset.name, line, ASSET_NAME_MAX - 1);
		asset.name[ASSET_NAME_MAX - 1] = '\0';

		if (pmemblk_write(pbp, &asset, assetid) < 0) {
			perror("pmemblk_write");
			exit(1);
		}

		assetid++;
	}

	free(line);
	fclose(fp);

	pmemblk_close(pbp);
	return 0;
}
