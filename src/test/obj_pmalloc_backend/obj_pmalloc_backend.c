/*
 * Copyright (c) 2015, Intel Corporation
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
 *     * Neither the name of Intel Corporation nor the names of its
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
 * obj_pmalloc_backend.c -- unit test for pmalloc backend interface
 */
#include <stdbool.h>
#include <assert.h>
#include "unittest.h"
#include "pmalloc.h"
#include "bucket.h"
#include "arena.h"
#include "backend.h"
#include "pool.h"
#include "backend_persistent.h"
#include "util.h"

#define	MOCK_BUCKET_OPS ((void *)0xABC)
#define	MOCK_ARENA_OPS ((void *)0xBCD)
#define	MOCK_POOL_OPS ((void *)0xCDE)

void
test_backend()
{
	struct backend mock_backend;
	backend_init(&mock_backend, BACKEND_NOOP, MOCK_BUCKET_OPS,
		MOCK_ARENA_OPS, MOCK_POOL_OPS);

	ASSERT(mock_backend.type == BACKEND_NOOP);
	ASSERT(mock_backend.b_ops == MOCK_BUCKET_OPS);
	ASSERT(mock_backend.a_ops == MOCK_ARENA_OPS);
	ASSERT(mock_backend.p_ops == MOCK_POOL_OPS);
}

#define	MOCK_POOL_SIZE 1024 * 256 * 100

void
test_verify_design_compliance()
{
	ASSERT(sizeof (struct backend_pool_header) == 1024);
	ASSERT(sizeof (struct backend_info_slot) == 32);
	ASSERT(sizeof (struct backend_info_slot_alloc) == 32);
	ASSERT(sizeof (struct backend_info_slot_realloc) == 32);
	ASSERT(sizeof (struct backend_info_slot_free) == 32);
	ASSERT(sizeof (struct backend_chunk_header) == 16);
}

void
test_backend_persistent_fresh_init()
{
	struct backend_pool *mock_pool = malloc(MOCK_POOL_SIZE);
	struct backend *mock_backend =
		backend_persistent_open(mock_pool, MOCK_POOL_SIZE);

	ASSERT(mock_pool->primary_header.state == POOL_STATE_OPEN);
	ASSERT(memcmp(mock_pool->primary_header.signature,
		POOL_SIGNATURE, POOL_SIGNATURE_LEN) == 0);

	ASSERT(memcmp(&mock_pool->zone[0].backup_header,
		&mock_pool->primary_header,
		sizeof (struct backend_pool_header)) == 0);

	for (int i = 0; i < MAX_INFO_SLOT; ++i) {
		ASSERT(mock_pool->info_slot[i].type == 0);
	}

	ASSERT(mock_backend != NULL);
	ASSERT(mock_backend->type == BACKEND_PERSISTENT);
	ASSERT(mock_backend->a_ops->set_alloc_ptr ==
		persistent_set_alloc_ptr);

	backend_persistent_close(mock_backend);
}

struct backend_pool_header valid_mock_hdr = {
	.signature = POOL_SIGNATURE,
	.state = POOL_STATE_CLOSED,
	.major = PERSISTENT_BACKEND_MAJOR,
	.size = MOCK_POOL_SIZE,
	.chunk_size = CHUNKSIZE,
	.chunks_per_zone = MAX_CHUNK,
	.reserved = {0},
	.checksum = 0
};

#define	MOCK_MINOR 999

void
test_backend_persistent_exisiting_closed_open()
{
	struct backend_pool *mock_pool = malloc(MOCK_POOL_SIZE);

	/*
	 * Write different minor version to verify that the header wasn't
	 * overwritten by pool open.
	 */
	valid_mock_hdr.minor = MOCK_MINOR;
	util_checksum(&valid_mock_hdr, sizeof (valid_mock_hdr),
		&valid_mock_hdr.checksum, 1);

	mock_pool->primary_header = valid_mock_hdr;
	struct backend *mock_backend =
		backend_persistent_open(mock_pool, MOCK_POOL_SIZE);

	ASSERT(mock_pool->primary_header.state == POOL_STATE_OPEN);
	ASSERT(mock_pool->primary_header.minor == MOCK_MINOR);
	ASSERT(mock_pool->zone[0].backup_header.minor == MOCK_MINOR);

	backend_persistent_close(mock_backend);
}

void
test_backend_persistent_recover_backup_open()
{
	struct backend_pool *mock_pool = malloc(MOCK_POOL_SIZE);

	valid_mock_hdr.minor = MOCK_MINOR;
	util_checksum(&valid_mock_hdr, sizeof (valid_mock_hdr),
		&valid_mock_hdr.checksum, 1);

	mock_pool->zone[0].backup_header = valid_mock_hdr;
	struct backend *mock_backend =
		backend_persistent_open(mock_pool, MOCK_POOL_SIZE);

	ASSERT(mock_pool->primary_header.state == POOL_STATE_OPEN);
	ASSERT(mock_pool->primary_header.minor == MOCK_MINOR);
	ASSERT(mock_pool->zone[0].backup_header.minor == MOCK_MINOR);

	backend_persistent_close(mock_backend);
}

#define	MOCK_DEST_ADDR 123

struct backend_info_slot_alloc mock_slot = {
	.type = INFO_SLOT_TYPE_ALLOC,
	.destination_addr = MOCK_DEST_ADDR
};

void
test_backend_persistent_open_slot_recovery_open()
{
	struct backend_pool *mock_pool = malloc(MOCK_POOL_SIZE);

	valid_mock_hdr.minor = MOCK_MINOR;
	valid_mock_hdr.state = POOL_STATE_OPEN;
	util_checksum(&valid_mock_hdr, sizeof (valid_mock_hdr),
		&valid_mock_hdr.checksum, 1);

	mock_pool->info_slot[0] = *((struct backend_info_slot *)&mock_slot);

	mock_pool->zone[0].backup_header = valid_mock_hdr;
	struct backend *mock_backend =
		backend_persistent_open(mock_pool, MOCK_POOL_SIZE);

	ASSERT(mock_pool->info_slot[0].type == 0);
	ASSERT(mock_pool->primary_header.state == POOL_STATE_OPEN);
	ASSERT(mock_pool->primary_header.minor == MOCK_MINOR);

	backend_persistent_close(mock_backend);
	ASSERT(mock_pool->primary_header.state == POOL_STATE_CLOSED);
}

void
test_backend_persistent_open_invalid_major()
{
	struct backend_pool *mock_pool = malloc(MOCK_POOL_SIZE);

	/*
	 * Write different minor version to verify that the header wasn't
	 * overwritten by pool open.
	 */
	valid_mock_hdr.major += 1;
	valid_mock_hdr.minor = MOCK_MINOR;
	util_checksum(&valid_mock_hdr, sizeof (valid_mock_hdr),
		&valid_mock_hdr.checksum, 1);

	mock_pool->primary_header = valid_mock_hdr;

	struct backend *mock_backend =
		backend_persistent_open(mock_pool, MOCK_POOL_SIZE);

	ASSERT(mock_backend == NULL);
}

void
test_backend_persistent_open_invalid_size()
{
	struct backend_pool *mock_pool = malloc(MOCK_POOL_SIZE);

	/*
	 * Write different minor version to verify that the header wasn't
	 * overwritten by pool open.
	 */
	valid_mock_hdr.size += 1;
	valid_mock_hdr.minor = MOCK_MINOR;
	util_checksum(&valid_mock_hdr, sizeof (valid_mock_hdr),
		&valid_mock_hdr.checksum, 1);

	mock_pool->primary_header = valid_mock_hdr;

	struct backend *mock_backend =
		backend_persistent_open(mock_pool, MOCK_POOL_SIZE);

	ASSERT(mock_backend == NULL);
}

#define	TEST_VAL_A 5
#define	TEST_VAL_B 10
uint64_t val = TEST_VAL_A;

bool mock_persist_called = false;

void
mock_persist(void *addr, size_t len)
{
	uint64_t *p_val = addr;
	ASSERT(p_val == &val);
	ASSERT(*p_val == TEST_VAL_B);
	mock_persist_called = true;
}

void
test_backend_persistent_set_ptr()
{
	struct backend_persistent mock_backend = {
		.persist = mock_persist
	};

	struct pmalloc_pool mock_pool = {
		.backend = (struct backend *)&mock_backend
	};

	struct arena mock_arena = {
		.pool = &mock_pool
	};

	persistent_set_alloc_ptr(&mock_arena, &val, TEST_VAL_B);
	ASSERT(val == TEST_VAL_B);
	ASSERT(mock_persist_called);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_pmalloc_backend");

	test_backend();
	test_verify_design_compliance();
	test_backend_persistent_fresh_init();
	test_backend_persistent_exisiting_closed_open();
	test_backend_persistent_recover_backup_open();
	test_backend_persistent_open_slot_recovery_open();
	test_backend_persistent_open_invalid_major();
	test_backend_persistent_open_invalid_size();

	test_backend_persistent_set_ptr();

	DONE(NULL);
}
