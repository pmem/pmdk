// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018, Intel Corporation */

/*
 * win_poolset_unmap.c -- test for windows mmap destructor.
 *
 * It checks whether all mappings are properly unmpapped and memory is properly
 * unreserved when auto growing pool is used.
 */

#include "unittest.h"
#include "os.h"
#include "libpmemobj.h"

#define KILOBYTE (1 << 10)
#define MEGABYTE (1 << 20)

#define LAYOUT_NAME "poolset_unmap"

int
main(int argc, char *argv[])
{
	START(argc, argv, "win_poolset_unmap");

	if (argc != 2)
		UT_FATAL("usage: %s path", argv[0]);

	PMEMobjpool *pop;
	if ((pop = pmemobj_create(argv[1], LAYOUT_NAME, 0,
		S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create");

	MEMORY_BASIC_INFORMATION basic_info;
	SIZE_T bytes_returned;
	SIZE_T offset = 0;

	bytes_returned = VirtualQuery(pop, &basic_info,
		sizeof(basic_info));

	/*
	 * When opening pool, we try to remove all permissions on header.
	 * If this action fails VirtualQuery will return one region with
	 * size 8MB. If it succeeds, RegionSize will be equal to 4KB due
	 * to different header and rest of the mapping permissions.
	 */
	if (basic_info.RegionSize == 4 * KILOBYTE) {
		/* header */
		UT_ASSERTeq(bytes_returned, sizeof(basic_info));
		UT_ASSERTeq(basic_info.State, MEM_COMMIT);

		offset += basic_info.RegionSize;

		/* first part */
		bytes_returned = VirtualQuery((char *)pop + offset, &basic_info,
			sizeof(basic_info));

		UT_ASSERTeq(bytes_returned, sizeof(basic_info));
		UT_ASSERTeq(basic_info.RegionSize, 8 * MEGABYTE - 4 * KILOBYTE);
		UT_ASSERTeq(basic_info.State, MEM_COMMIT);
	} else {
		/* first part with header */
		UT_ASSERTeq(bytes_returned, sizeof(basic_info));
		UT_ASSERTeq(basic_info.RegionSize, 8 * MEGABYTE);
		UT_ASSERTeq(basic_info.State, MEM_COMMIT);
	}

	offset += basic_info.RegionSize;

	/* reservation after first part */
	bytes_returned = VirtualQuery((char *)pop + offset, &basic_info,
		sizeof(basic_info));

	UT_ASSERTeq(bytes_returned, sizeof(basic_info));
	UT_ASSERTeq(basic_info.RegionSize, (50 - 8) * MEGABYTE);
	UT_ASSERTeq(basic_info.State, MEM_RESERVE);

	DONE(NULL);
}
