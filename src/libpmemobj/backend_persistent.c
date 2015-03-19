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
 * backend_persistent.c -- implementation of persistent backend
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <libpmem.h>
#include <string.h>
#include "bucket.h"
#include "arena.h"
#include "backend.h"
#include "pool.h"
#include "util.h"
#include "out.h"
#include "backend_persistent.h"

static struct bucket_backend_operations persistent_bucket_ops = {

};

static struct arena_backend_operations persistent_arena_ops = {
	.set_alloc_ptr = persistent_set_alloc_ptr
};

static struct pool_backend_operations persistent_pool_ops = {

};

/*
 * verify_header -- (internal) check is the header is consistent
 */
static bool
verify_header(struct backend_pool_header *h)
{
	if (util_checksum(h, sizeof (*h), &h->checksum, 0) != 1) {
		return false;
	}

	if (memcmp(h->signature, POOL_SIGNATURE, POOL_SIGNATURE_LEN) != 0) {
		return false;
	}

	return true;
}

/*
 * copy_header -- (internal) create a copy of a header
 */
static void
copy_header(struct backend_persistent *b, struct backend_pool_header *left,
	struct backend_pool_header *right)
{
	memcpy(left, right, sizeof (*left));
	b->persist(left, sizeof (*left));
}

/*
 * recover_primary_header -- (internal) check backups for a valid header copy
 */
static bool
recover_primary_header(struct backend_persistent *b)
{
	for (int i = 0; i < b->max_zone; ++i) {
		if (verify_header(&b->pool->zone[i].backup_header)) {
			copy_header(b, &b->pool->primary_header,
				&b->pool->zone[i].backup_header);
			return true;
		}
	}

	return false;
}

/*
 * zero_info_slots -- (internal) zerofill all info slot structures
 */
static void
zero_info_slots(struct backend_persistent *b)
{
	memset(b->pool->info_slot, 0, sizeof (b->pool->info_slot));
	b->persist(b->pool->info_slot, sizeof (b->pool->info_slot));
}

/*
 * write_primary_pool_header -- (internal) create a fresh pool header
 */
static void
write_primary_pool_header(struct backend_persistent *b)
{
	struct backend_pool_header *hdrp = &b->pool->primary_header;
	memcpy(hdrp->signature, POOL_SIGNATURE, POOL_SIGNATURE_LEN);
	hdrp->flags = 0;
	hdrp->state = POOL_STATE_CLOSED;
	hdrp->major = PERSISTENT_BACKEND_MAJOR;
	hdrp->minor = PERSISTENT_BACKEND_MINOR;
	hdrp->size = b->pool_size;
	hdrp->chunk_size = CHUNKSIZE;
	hdrp->chunks_per_zone = MAX_CHUNK;
	memset(hdrp->reserved, 0, sizeof (hdrp->reserved));
	hdrp->checksum = 0;
	b->persist(hdrp, sizeof (*hdrp));
	util_checksum(hdrp, sizeof (*hdrp), &hdrp->checksum, 1);
	b->persist(&hdrp->checksum, sizeof (hdrp->checksum));
}

/*
 * write_backup_pool_headers -- (internal) copy primary header into backups
 */
static void
write_backup_pool_headers(struct backend_persistent *b)
{
	for (int i = 0; i < b->max_zone; ++i) {
		copy_header(b, &b->pool->zone[i].backup_header,
			&b->pool->primary_header);
	}
}

/*
 * write_pool_layout -- (internal) create a fresh pool layout
 */
static void
write_pool_layout(struct backend_persistent *b)
{
	zero_info_slots(b);
	write_primary_pool_header(b);
	write_backup_pool_headers(b);
}

/*
 * get_pool_state -- (internal) returns state of the pool
 */
static enum pool_state
get_pool_state(struct backend_persistent *b)
{
	return b->pool->primary_header.state;
}

/*
 * set_pool_state -- (internal) change pool state
 *
 * Writes the state into the primary header first and then waterfalls it into
 * all of the backups.
 */
static void
set_pool_state(struct backend_persistent *b, enum pool_state state)
{
	struct backend_pool_header *hdrp = &b->pool->primary_header;
	hdrp->state = state;
	util_checksum(hdrp, sizeof (*hdrp), &hdrp->checksum, 1);
	b->persist(&hdrp->checksum, sizeof (hdrp->checksum));
	write_backup_pool_headers(b);
}

/*
 * Recover slot functions are all flushed using a single persist call, and so
 * they have to be implemented in a way that is resistent to store reordering.
 */

/*
 * recover_slot_unknown -- (internal) clear already recovered slot
 *
 * This function will be called on slots that have been already processed but
 * the clearing function was interrupted at the last moment.
 */
static void
recover_slot_unknown(struct backend_persistent *b,
	struct backend_info_slot *slot)
{
	/*
	 * The slot was already being discarded, just get rid of any
	 * potential left-overs and all will be OK.
	 */
	memset(slot, 0, sizeof (*slot));
	b->persist(slot, sizeof (*slot));
}

/*
 * recover_slot_alloc -- (internal) Revert not completed allocation
 */
static void
recover_slot_alloc(struct backend_persistent *b,
	struct backend_info_slot *slot)
{
	struct backend_info_slot_alloc *alloc_slot =
		(struct backend_info_slot_alloc *)slot;

	/* XXX after guard implementation */

	memset(alloc_slot, 0, sizeof (*alloc_slot));
	b->persist(alloc_slot, sizeof (*alloc_slot));
}

/*
 * recover_slot_realloc -- (internal) Revert not completed reallocation
 */
static void
recover_slot_realloc(struct backend_persistent *b,
	struct backend_info_slot *slot)
{
	struct backend_info_slot_realloc *realloc_slot =
		(struct backend_info_slot_realloc *)slot;

	/* XXX after guard implementation */

	memset(realloc_slot, 0, sizeof (*realloc_slot));
	b->persist(realloc_slot, sizeof (*realloc_slot));
}

/*
 * recover_slot_free -- (internal) Revert not completed free
 */
static void
recover_slot_free(struct backend_persistent *b,
	struct backend_info_slot *slot)
{
	struct backend_info_slot_free *free_slot =
		(struct backend_info_slot_free *)slot;

	/* XXX after guard implementation */

	memset(free_slot, 0, sizeof (*free_slot));
	b->persist(free_slot, sizeof (*free_slot));
}

static void (*recover_slot[MAX_INFO_SLOT_TYPE])(struct backend_persistent *b,
	struct backend_info_slot *slot) = {
	recover_slot_unknown,
	recover_slot_alloc,
	recover_slot_realloc,
	recover_slot_free
};

/*
 * recover_info_slot -- (internal) Choose recovery function based on a slot type
 */
static void
recover_info_slot(struct backend_persistent *b,
	struct backend_info_slot *slot)
{
	ASSERT(slot->type < MAX_INFO_SLOT_TYPE);
	ASSERT(recover_slot[slot->type] != NULL);

	/*
	 * Call the appropriate recover function, but only if the slot isn't
	 * empty.
	 */
	static const char empty_info_slot[INFO_SLOT_DATA_SIZE] = {0};
	if (slot->type != 0 &&
		memcmp(empty_info_slot, slot->data, INFO_SLOT_DATA_SIZE)) {
		recover_slot[slot->type](b, slot);
	}
}

/*
 * can_open_pool -- (internal) Check if the pool can be opened by this build
 */
static bool
can_open_pool(struct backend_persistent *b)
{
	struct backend_pool_header h = b->pool->primary_header;
	if (h.size != b->pool_size) {
		LOG(3, "Trying to open valid pool with mismatched size");
		return false;
	}

	if (h.major != PERSISTENT_BACKEND_MAJOR) {
		LOG(3, "Trying to open pool created with incompatible backend "
			"version");
		return false;
	}

	if (h.chunk_size != CHUNKSIZE) {
		LOG(3, "Trying to open pool with chunksize different than %u. "
			"This is a compile-time constant.", CHUNKSIZE);
		return false;
	}

	if (h.chunks_per_zone != MAX_CHUNK) {
		LOG(3, "Trying to open pool with chunks per zone different "
		"than %lu. This is a compile-time constant.", MAX_CHUNK);
		return false;
	}

	return true;
}

/*
 * open_pmem_storage -- (internal) Open the actual persistent pool memory region
 */
static bool
open_pmem_storage(struct backend_persistent *b)
{
	ASSERT(b->pool != NULL);
	ASSERT(b->pool_size > 0);
	uint64_t zone_max_size = sizeof (struct backend_zone) +
		(MAX_CHUNK * CHUNKSIZE);

	size_t rawsize = b->pool_size;
	b->max_zone = 0;
	while (rawsize > ZONE_MIN_SIZE) {
		b->max_zone++;
		rawsize -= rawsize < zone_max_size ? rawsize : zone_max_size;
	}

	bool pool_valid = verify_header(&b->pool->primary_header) ||
		recover_primary_header(b);

	if (pool_valid) {
		/*
		 * The pool is valid but may be incompatible with this
		 * implementation.
		 */
		if (!can_open_pool(b))
			return false;
	} else {
		write_pool_layout(b);
	}

	switch (get_pool_state(b)) {
	case POOL_STATE_CLOSED:
#ifdef DEBUG
		for (int i = 0; i < MAX_INFO_SLOT; ++i) {
			ASSERT(b->pool->info_slot[i].type == 0);
		}
#endif
		/* all is good */
		set_pool_state(b, POOL_STATE_OPEN);
		return true;
	case POOL_STATE_OPEN:
		/* need to iterate through info slots */
		for (int i = 0; i < MAX_INFO_SLOT; ++i) {
			recover_info_slot(b, &b->pool->info_slot[i]);
		}
		/* copy primary header into all backups, just in case */
		write_backup_pool_headers(b);
		return true;
	default:
		ASSERT(false); /* code unreachable */
	}

	return false;
}

/*
 * close_pmem_storage -- (internal) Close persistent memory pool region
 */
static void
close_pmem_storage(struct backend_persistent *b)
{
	/*
	 * Closing a pool with threads still using it is forbidden,
	 * check this only for debug build.
	 */
#ifdef DEBUG
	for (int i = 0; i < MAX_INFO_SLOT; ++i) {
		ASSERT(b->pool->info_slot[i].type == 0);
	}
#endif
	ASSERT(get_pool_state(b) == POOL_STATE_OPEN);

	set_pool_state(b, POOL_STATE_CLOSED);
}

/*
 * persistent_backend_open -- opens a persistent backend
 */
struct backend *
backend_persistent_open(void *ptr, size_t size)
{
	struct backend_persistent *backend = Malloc(sizeof (*backend));
	if (backend == NULL) {
		goto error_backend_malloc;
	}

	backend_init(&(backend->super), BACKEND_PERSISTENT,
		&persistent_bucket_ops,
		&persistent_arena_ops,
		&persistent_pool_ops);

	backend->is_pmem = pmem_is_pmem(ptr, size);
	/*
	 * Casting is necessery to silence the compiler about msync returning
	 * an int instead of nothing - the return value is not used anyway.
	 */
	if (backend->is_pmem)
		backend->persist = (persist_func)pmem_persist;
	else
		backend->persist = (persist_func)pmem_msync;

	backend->pool = ptr;
	backend->pool_size = size;
	if (!open_pmem_storage(backend)) {
		goto error_pool_open;
	}

	return (struct backend *)backend;

error_pool_open:
	Free(backend);
error_backend_malloc:
	return NULL;
}

/*
 * persistent_backend_close -- closes a persistent backend
 */
void
backend_persistent_close(struct backend *backend)
{
	ASSERT(backend->type == BACKEND_PERSISTENT);

	struct backend_persistent *persistent_backend =
		(struct backend_persistent *)backend;

	close_pmem_storage(persistent_backend);
	Free(persistent_backend);
}

/*
 * persistent_set_alloc_ptr -- persistent implementation of set_alloc_ptr
 */
void
persistent_set_alloc_ptr(struct arena *arena, uint64_t *ptr,
	uint64_t value)
{
	struct backend_persistent *backend =
		(struct backend_persistent *)arena->pool->backend;

	*ptr = value;
	backend->persist(ptr, sizeof (*ptr));
}
