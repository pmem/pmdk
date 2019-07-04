/*
 * Copyright 2016-2019, Intel Corporation
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
 * obj_layout.c -- unit test for layout
 *
 * This test should be modified after every layout change. It's here to prevent
 * any accidental layout changes.
 */
#include "util.h"
#include "unittest.h"

#include "sync.h"
#include "heap_layout.h"
#include "lane.h"
#include "tx.h"
#include "ulog.h"
#include "list.h"

#define SIZEOF_CHUNK_HEADER_V3 (8)
#define MAX_CHUNK_V3 (65535 - 7)
#define SIZEOF_CHUNK_V3 (1024ULL * 256)
#define SIZEOF_CHUNK_RUN_HEADER_V3 (16)
#define SIZEOF_ZONE_HEADER_V3 (64)
#define SIZEOF_ZONE_METADATA_V3 (SIZEOF_ZONE_HEADER_V3 +\
	SIZEOF_CHUNK_HEADER_V3 * MAX_CHUNK_V3)
#define SIZEOF_HEAP_HDR_V3 (1024)
#define SIZEOF_LEGACY_ALLOCATION_HEADER_V3 (64)
#define SIZEOF_COMPACT_ALLOCATION_HEADER_V3 (16)
#define SIZEOF_LOCK_V3 (64)
#define SIZEOF_PMEMOID_V3 (16)
#define SIZEOF_LIST_ENTRY_V3 (SIZEOF_PMEMOID_V3 * 2)
#define SIZEOF_LIST_HEAD_V3 (SIZEOF_PMEMOID_V3 + SIZEOF_LOCK_V3)
#define SIZEOF_LANE_SECTION_V3 (1024)
#define SIZEOF_LANE_V3 (3 * SIZEOF_LANE_SECTION_V3)
#define SIZEOF_ULOG_V4 (64)
#define SIZEOF_ULOG_BASE_ENTRY_V4 (8)
#define SIZEOF_ULOG_VAL_ENTRY_V4 (16)
#define SIZEOF_ULOG_BUF_ENTRY_V4 (24)

POBJ_LAYOUT_BEGIN(layout);
POBJ_LAYOUT_ROOT(layout, struct foo);
POBJ_LAYOUT_END(layout);

struct foo {
	POBJ_LIST_ENTRY(struct foo) f;
};

POBJ_LIST_HEAD(foo_head, struct foo);

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_layout");

	UT_COMPILE_ERROR_ON(CHUNKSIZE != SIZEOF_CHUNK_V3);

	ASSERT_ALIGNED_BEGIN(struct chunk);
	ASSERT_ALIGNED_FIELD(struct chunk, data);
	ASSERT_ALIGNED_CHECK(struct chunk);
	UT_COMPILE_ERROR_ON(sizeof(struct chunk_run) != SIZEOF_CHUNK_V3);

	ASSERT_ALIGNED_BEGIN(struct chunk_run_header);
	ASSERT_ALIGNED_FIELD(struct chunk_run_header, block_size);
	ASSERT_ALIGNED_FIELD(struct chunk_run_header, alignment);
	ASSERT_ALIGNED_CHECK(struct chunk_run_header);
	UT_COMPILE_ERROR_ON(sizeof(struct chunk_run_header) !=
		SIZEOF_CHUNK_RUN_HEADER_V3);

	ASSERT_ALIGNED_BEGIN(struct chunk_run);
	ASSERT_ALIGNED_FIELD(struct chunk_run, hdr);
	ASSERT_ALIGNED_FIELD(struct chunk_run, content);
	ASSERT_ALIGNED_CHECK(struct chunk_run);
	UT_COMPILE_ERROR_ON(sizeof(struct chunk_run) != SIZEOF_CHUNK_V3);

	ASSERT_ALIGNED_BEGIN(struct chunk_header);
	ASSERT_ALIGNED_FIELD(struct chunk_header, type);
	ASSERT_ALIGNED_FIELD(struct chunk_header, flags);
	ASSERT_ALIGNED_FIELD(struct chunk_header, size_idx);
	ASSERT_ALIGNED_CHECK(struct chunk_header);
	UT_COMPILE_ERROR_ON(sizeof(struct chunk_header) !=
		SIZEOF_CHUNK_HEADER_V3);

	ASSERT_ALIGNED_BEGIN(struct zone_header);
	ASSERT_ALIGNED_FIELD(struct zone_header, magic);
	ASSERT_ALIGNED_FIELD(struct zone_header, size_idx);
	ASSERT_ALIGNED_FIELD(struct zone_header, reserved);
	ASSERT_ALIGNED_CHECK(struct zone_header);
	UT_COMPILE_ERROR_ON(sizeof(struct zone_header) !=
		SIZEOF_ZONE_HEADER_V3);

	ASSERT_ALIGNED_BEGIN(struct zone);
	ASSERT_ALIGNED_FIELD(struct zone, header);
	ASSERT_ALIGNED_FIELD(struct zone, chunk_headers);
	ASSERT_ALIGNED_CHECK(struct zone);
	UT_COMPILE_ERROR_ON(sizeof(struct zone) !=
		SIZEOF_ZONE_METADATA_V3);

	ASSERT_ALIGNED_BEGIN(struct heap_header);
	ASSERT_ALIGNED_FIELD(struct heap_header, signature);
	ASSERT_ALIGNED_FIELD(struct heap_header, major);
	ASSERT_ALIGNED_FIELD(struct heap_header, minor);
	ASSERT_ALIGNED_FIELD(struct heap_header, unused);
	ASSERT_ALIGNED_FIELD(struct heap_header, chunksize);
	ASSERT_ALIGNED_FIELD(struct heap_header, chunks_per_zone);
	ASSERT_ALIGNED_FIELD(struct heap_header, reserved);
	ASSERT_ALIGNED_FIELD(struct heap_header, checksum);
	ASSERT_ALIGNED_CHECK(struct heap_header);
	UT_COMPILE_ERROR_ON(sizeof(struct heap_header) !=
		SIZEOF_HEAP_HDR_V3);

	ASSERT_ALIGNED_BEGIN(struct allocation_header_legacy);
	ASSERT_ALIGNED_FIELD(struct allocation_header_legacy, unused);
	ASSERT_ALIGNED_FIELD(struct allocation_header_legacy, size);
	ASSERT_ALIGNED_FIELD(struct allocation_header_legacy, unused2);
	ASSERT_ALIGNED_FIELD(struct allocation_header_legacy, root_size);
	ASSERT_ALIGNED_FIELD(struct allocation_header_legacy, type_num);
	ASSERT_ALIGNED_CHECK(struct allocation_header_legacy);
	UT_COMPILE_ERROR_ON(sizeof(struct allocation_header_legacy) !=
		SIZEOF_LEGACY_ALLOCATION_HEADER_V3);

	ASSERT_ALIGNED_BEGIN(struct allocation_header_compact);
	ASSERT_ALIGNED_FIELD(struct allocation_header_compact, size);
	ASSERT_ALIGNED_FIELD(struct allocation_header_compact, extra);
	ASSERT_ALIGNED_CHECK(struct allocation_header_compact);
	UT_COMPILE_ERROR_ON(sizeof(struct allocation_header_compact) !=
		SIZEOF_COMPACT_ALLOCATION_HEADER_V3);

	ASSERT_ALIGNED_BEGIN(struct ulog);
	ASSERT_ALIGNED_FIELD(struct ulog, checksum);
	ASSERT_ALIGNED_FIELD(struct ulog, next);
	ASSERT_ALIGNED_FIELD(struct ulog, capacity);
	ASSERT_ALIGNED_FIELD(struct ulog, gen_num);
	ASSERT_ALIGNED_FIELD(struct ulog, flags);
	ASSERT_ALIGNED_FIELD(struct ulog, unused);
	ASSERT_ALIGNED_CHECK(struct ulog);
	UT_COMPILE_ERROR_ON(sizeof(struct ulog) !=
		SIZEOF_ULOG_V4);

	ASSERT_ALIGNED_BEGIN(struct ulog_entry_base);
	ASSERT_ALIGNED_FIELD(struct ulog_entry_base, offset);
	ASSERT_ALIGNED_CHECK(struct ulog_entry_base);
	UT_COMPILE_ERROR_ON(sizeof(struct ulog_entry_base) !=
		SIZEOF_ULOG_BASE_ENTRY_V4);

	ASSERT_ALIGNED_BEGIN(struct ulog_entry_val);
	ASSERT_ALIGNED_FIELD(struct ulog_entry_val, base);
	ASSERT_ALIGNED_FIELD(struct ulog_entry_val, value);
	ASSERT_ALIGNED_CHECK(struct ulog_entry_val);
	UT_COMPILE_ERROR_ON(sizeof(struct ulog_entry_val) !=
		SIZEOF_ULOG_VAL_ENTRY_V4);

	ASSERT_ALIGNED_BEGIN(struct ulog_entry_buf);
	ASSERT_ALIGNED_FIELD(struct ulog_entry_buf, base);
	ASSERT_ALIGNED_FIELD(struct ulog_entry_buf, checksum);
	ASSERT_ALIGNED_FIELD(struct ulog_entry_buf, size);
	ASSERT_ALIGNED_CHECK(struct ulog_entry_buf);
	UT_COMPILE_ERROR_ON(sizeof(struct ulog_entry_buf) !=
		SIZEOF_ULOG_BUF_ENTRY_V4);

	ASSERT_ALIGNED_BEGIN(PMEMoid);
	ASSERT_ALIGNED_FIELD(PMEMoid, pool_uuid_lo);
	ASSERT_ALIGNED_FIELD(PMEMoid, off);
	ASSERT_ALIGNED_CHECK(PMEMoid);
	UT_COMPILE_ERROR_ON(sizeof(PMEMoid) !=
		SIZEOF_PMEMOID_V3);

	UT_COMPILE_ERROR_ON(sizeof(PMEMmutex) != SIZEOF_LOCK_V3);
	UT_COMPILE_ERROR_ON(sizeof(PMEMmutex) != sizeof(PMEMmutex_internal));
	UT_COMPILE_ERROR_ON(util_alignof(PMEMmutex) !=
		util_alignof(PMEMmutex_internal));
	UT_COMPILE_ERROR_ON(util_alignof(PMEMmutex) !=
		util_alignof(os_mutex_t));
	UT_COMPILE_ERROR_ON(util_alignof(PMEMmutex) !=
		util_alignof(uint64_t));

	UT_COMPILE_ERROR_ON(sizeof(PMEMrwlock) != SIZEOF_LOCK_V3);
	UT_COMPILE_ERROR_ON(util_alignof(PMEMrwlock) !=
		util_alignof(PMEMrwlock_internal));
	UT_COMPILE_ERROR_ON(util_alignof(PMEMrwlock) !=
		util_alignof(os_rwlock_t));
	UT_COMPILE_ERROR_ON(util_alignof(PMEMrwlock) !=
		util_alignof(uint64_t));

	UT_COMPILE_ERROR_ON(sizeof(PMEMcond) != SIZEOF_LOCK_V3);
	UT_COMPILE_ERROR_ON(util_alignof(PMEMcond) !=
		util_alignof(PMEMcond_internal));
	UT_COMPILE_ERROR_ON(util_alignof(PMEMcond) !=
		util_alignof(os_cond_t));
	UT_COMPILE_ERROR_ON(util_alignof(PMEMcond) !=
		util_alignof(uint64_t));

	UT_COMPILE_ERROR_ON(sizeof(struct foo) != SIZEOF_LIST_ENTRY_V3);
	UT_COMPILE_ERROR_ON(sizeof(struct list_entry) != SIZEOF_LIST_ENTRY_V3);
	UT_COMPILE_ERROR_ON(sizeof(struct foo_head) != SIZEOF_LIST_HEAD_V3);
	UT_COMPILE_ERROR_ON(sizeof(struct list_head) != SIZEOF_LIST_HEAD_V3);

	ASSERT_ALIGNED_BEGIN(struct lane_layout);
	ASSERT_ALIGNED_FIELD(struct lane_layout, internal);
	ASSERT_ALIGNED_FIELD(struct lane_layout, external);
	ASSERT_ALIGNED_FIELD(struct lane_layout, undo);
	ASSERT_ALIGNED_CHECK(struct lane_layout);
	UT_COMPILE_ERROR_ON(sizeof(struct lane_layout) !=
		SIZEOF_LANE_V3);


	DONE(NULL);
}

#ifdef _MSC_VER
/*
 * Since libpmemobj is linked statically, we need to invoke its ctor/dtor.
 */
MSVC_CONSTR(libpmemobj_init)
MSVC_DESTR(libpmemobj_fini)
#endif
