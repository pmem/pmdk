// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/*
 * basic.c -- a simple example for the libpmemset library
 * that shows the use of basic libpmemset API.
 * This example creates a source, maps it, writes to it,
 * and persists the data.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libpmem2.h>
#include <libpmemset.h>

#define PART_SIZE 131072 /* 128 KiB */
#define PART_OFFSET 65536 /* 64 KiB */
#define NUMBER_OF_PARTS 3

int
main(int argc, char *argv[])
{
	struct pmemset_source *src;
	struct pmemset_config *cfg;
	struct pmemset *set;
	struct pmemset_map_config *map_cfg;
	struct pmemset_part_descriptor desc[NUMBER_OF_PARTS];
	struct pmemset_part_map *part_maps[NUMBER_OF_PARTS];

	/* Parse and validate input arguments, file path is obligatory. */
	if (argc != 2) {
		fprintf(stderr, "usage: %s file\n", argv[0]);
		exit(1);
	}

	char *file = argv[1];
	int ret;

	/*
	 * This function creates pmemset source using a file path.
	 *
	 * The libpmemset library also allows creating source
	 * using file descriptor, pmem2 source, or temporary file.
	 * For more details, see pmemset_source_from_fd(3),
	 * pmemset_source_from_pmem2(3) and pmemset_source_from_temporary(3)
	 * man pages.
	 */
	ret = pmemset_source_from_file(&src, file);
	if (ret) {
		pmemset_perror("pmemset_source_from_file");
		exit(1);
	}

	/*
	 * This function initializes the config of the entire set.
	 * The only required parameter in the set configuration is
	 * granularity. But more attributes can be specified eg.
	 * memory reservation, events, acceptable part states,
	 * part coalescing.
	 *
	 * For more details about the granularity concept, especially other
	 * types like PMEM2_GRANULARITY_CACHE_LINE, PMEM2_GRANULARITY_BYTE see
	 * libpmem2(7) man page.
	 */
	ret = pmemset_config_new(&cfg);
	if (ret) {
		pmemset_perror("pmemset_config_new");
		goto exit_src_del;
	}

	/* Set required store granularity in the config. */
	enum pmem2_granularity graunlarity = PMEM2_GRANULARITY_PAGE;
	ret = pmemset_config_set_required_store_granularity(cfg, graunlarity);
	if (ret) {
		pmemset_perror("pmemset_config_set_required_store_granularity");
		goto exit_cfg_del;
	}

	/* Create a new set object using previously defined config. */
	ret = pmemset_new(&set, cfg);
	if (ret) {
		pmemset_perror("pmemset_new");
		goto exit_cfg_del;
	}

	/*
	 * This function creates new map configuration.
	 *
	 * Map configuration is not required to create a new mapping,
	 * however, can extend the functionality by defining the mapping length
	 * and offset in the file. By default entire size of file would be used.
	 */
	ret = pmemset_map_config_new(&map_cfg);
	if (ret) {
		pmemset_perror("pmemset_map_config_new");
		goto exit_set_del;
	}

	/* Configure the size of the new mapped part. */
	pmemset_map_config_set_length(map_cfg, PART_SIZE);

	/*
	 * Offset always has to be align to the source alignment.
	 * To read the alignment for specified source use
	 * pmemset_source_alignment(3) API.
	 */
	size_t alignment;
	if (pmemset_source_alignment(src, &alignment)) {
		pmemset_perror("pmemset_source_alignment");
		goto exit_set_del;
	}

	if (PART_OFFSET % alignment != 0) {
		fprintf(stderr, "Offset is not aligned");
		goto exit_set_del;
	}

	/*
	 * Map a few parts based on the prepared configuration.
	 *
	 * The last parameter to the pmemset_map function is also optional.
	 * It represents part descriptor - structure describing the
	 * created mapping. There is also another way to read
	 * information about this structure, using the function
	 * pmemset_descriptor_part_map(3) directly on created part_map,
	 * as in the example below.
	 */
	for (int i = 0; i < NUMBER_OF_PARTS; i++) {
		/* Configure the offset for each mapped part. */
		pmemset_map_config_set_offset(map_cfg, PART_OFFSET * i);
		ret = pmemset_map(set, src, map_cfg, NULL);
		if (ret) {
			pmemset_perror("pmemset_map");
			goto exit;
		}
	}

	/* Get first part map from the set. */
	pmemset_first_part_map(set, &part_maps[0]);
	if (NULL != part_maps[0]) {
		/* Read descriptor of the first part map. */
		desc[0] = pmemset_descriptor_part_map(part_maps[0]);
	}

	/*
	 * Read descriptors of all part maps using the pmemset_next_part_map
	 * function. This function takes the current part map and returns
	 * the next one by parameter.
	 *
	 * In the libpmemset API exists additional function to
	 * find part map object in the set,
	 * it is pmemset_part_map_by_address(3).
	 * For more information see the man page of this function.
	 */
	for (int i = 1; i < NUMBER_OF_PARTS; i++) {
		pmemset_next_part_map(set, part_maps[i - 1], &part_maps[i]);
		desc[i] = pmemset_descriptor_part_map(part_maps[i]);
	}

	/*
	 * At this point, all descriptors are known and any operation can be
	 * easily performed. In this example, data is written to each part,
	 * persisted and read.
	 */
	char text[1024];
	for (int i = 0; i < NUMBER_OF_PARTS; i++) {
		snprintf(text, sizeof(text),
			"PMDK libpmemset part map number %d", i);
		strcpy(desc[i].addr, text);
		pmemset_persist(set, desc[i].addr, strlen(text) + 1);
		if (ret) {
			pmemset_perror("pmemset_persist");
			goto exit;
		}
		printf("%s\n", (char *)desc[i].addr);
	}

exit:
	pmemset_map_config_delete(&map_cfg);
exit_set_del:
	pmemset_delete(&set);
exit_cfg_del:
	pmemset_config_delete(&cfg);
exit_src_del:
	pmemset_source_delete(&src);

	return ret;
}
