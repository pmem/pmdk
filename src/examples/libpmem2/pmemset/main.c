// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

#include "pmemset.h"
#include <stdio.h>
#include <assert.h>

int
main(int argc, char *argv[])
{
	struct pmemset_config *config;
	pmemset_config_new(&config);
	pmemset_config_set_create_if_none(config, 1);

	struct pmemset_source *source;
	pmemset_source_from_file(&source, "/mnt/pmem/testfile");

	struct pmemset *set;
	pmemset_new(&set, config);

	struct pmemset_part *part;
	pmemset_part_new(&part, set, source, 0, 1 << 20);

	enum pmemset_part_state state;
	struct pmemset_part_map *pmap;
	pmemset_part_map_new(&pmap, &part, NULL, NULL, NULL, NULL, &state);
	assert(state == PMEMSET_PART_STATE_OK);

	struct pmemset_part_map *first;
	pmemset_part_map_first(set, &first);
	assert(pmap == first);

	void *addr = pmemset_part_map_address(pmap);
	size_t len = pmemset_part_map_length(pmap);

	/*
	 * Even though these two are identical pointers, they need to be
	 * dropped separately since part maps are reference counted.
	 */
	pmemset_part_map_drop(&pmap);
	pmemset_part_map_drop(&first);

	pmemset_memset(set, addr, 0xc, len, 0);

	pmemset_config_delete(&config);
	pmemset_source_delete(&source);
	pmemset_delete(&set);

	return 0;
}
