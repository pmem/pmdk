/*
 * Copyright 2016, Intel Corporation
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
 * obj_memblock.c -- unit test for memblock interface
 */
#include "unittest.h"
#include "util.h"
#include "lane.h"
#include "redo.h"
#include "memops.h"
#include "pmalloc.h"
#include "list.h"
#include "obj.h"
#include "heap_layout.h"
#include "memblock.h"
#include "heap.h"

#define NCHUNKS 3

static PMEMobjpool *pop;

static void
test_detect()
{
	struct memory_block mhuge_used = { .chunk_id = 0, 0, 0, 0 };
	struct memory_block mhuge_free = { .chunk_id = 1, 0, 0, 0 };
	struct memory_block mrun = { .chunk_id = 2, 0, 0, 0 };

	pop->hlayout->zone0.chunk_headers[0].size_idx = 1;
	pop->hlayout->zone0.chunk_headers[0].type = CHUNK_TYPE_USED;

	pop->hlayout->zone0.chunk_headers[1].size_idx = 1;
	pop->hlayout->zone0.chunk_headers[1].type = CHUNK_TYPE_FREE;

	pop->hlayout->zone0.chunk_headers[2].size_idx = 1;
	pop->hlayout->zone0.chunk_headers[2].type = CHUNK_TYPE_RUN;

	UT_ASSERTeq(memblock_autodetect_type(&mhuge_used, pop->hlayout),
		MEMORY_BLOCK_HUGE);
	UT_ASSERTeq(memblock_autodetect_type(&mhuge_free, pop->hlayout),
		MEMORY_BLOCK_HUGE);
	UT_ASSERTeq(memblock_autodetect_type(&mrun, pop->hlayout),
		MEMORY_BLOCK_RUN);
}

static void
test_block_size()
{
	struct memory_block mhuge = { .chunk_id = 0, 0, 0, 0 };
	struct memory_block mrun = { .chunk_id = 1, 0, 0, 0 };
	pop->hlayout->zone0.chunk_headers[0].size_idx = 1;
	pop->hlayout->zone0.chunk_headers[0].type = CHUNK_TYPE_USED;

	pop->hlayout->zone0.chunk_headers[1].size_idx = 1;
	pop->hlayout->zone0.chunk_headers[1].type = CHUNK_TYPE_RUN;
	struct chunk_run *run = (struct chunk_run *)
		&pop->hlayout->zone0.chunks[1];
	run->block_size = 1234;

	UT_ASSERTeq(MEMBLOCK_OPS(, &mhuge)->block_size(&mhuge, pop->hlayout),
		CHUNKSIZE);
	UT_ASSERTeq(MEMBLOCK_OPS(, &mrun)->block_size(&mrun, pop->hlayout),
		1234);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_memblock");
	PMEMobjpool pool;
	pop = &pool;

	pop->hlayout = ZALLOC(sizeof(struct heap_layout) +
		NCHUNKS * sizeof(struct chunk));

	test_detect();
	test_block_size();

	DONE(NULL);
}
