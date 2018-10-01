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
