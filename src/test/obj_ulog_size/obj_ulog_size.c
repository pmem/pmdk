/*
 * Copyright 2019, Intel Corporation
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
 * obj_ulog_size.c -- unit tests for pmemobj_action API and
 *		redo, undo logs
 */
#include <sys/param.h>
#include <string.h>
#include <stddef.h>

#include "unittest.h"

/*
 * lane.h -- needed for LANE_REDO_EXTERNAL_SIZE
 */
#include "lane.h"

/* TX_INTENT_LOG_ENTRY_OVERHEAD -- sizeof(struct ulog_entry_val) */
#define TX_INTENT_LOG_ENTRY_OVERHEAD (0b01ULL << 4) /* 16 bytes */
#define TX_SNAPSHOT_ALIGNMENT CACHELINE_SIZE
#define TX_SNAPSHOT_LOG_BUFFER_OVERHEAD 64ULL
/* TX_INTENT_LOG_ENTRY_OVERHEAD -- sizeof(struct ulog_entry_buf) */
#define TX_SNAPSHOT_LOG_ENTRY_OVERHEAD (0b11ULL << 3) /* 24 */
#define TX_INTENT_ALIGNMENT CACHELINE_SIZE
#define TX_INTENT_LOG_BUFFER_OVERHEAD 64ULL

#define LAYOUT_NAME "obj_ulog_size"
#define TEST_VALUE_1 1
#define TEST_VALUE_2 2

struct object {
	size_t value;
};

TOID_DECLARE(struct object, 0);
#define MIN_ALLOC 64
#define MB (1024 * 1024)
#define MAX_OBJECTS (16 * MB / MIN_ALLOC)
#define DIVIDER 2

/*
 * REDO_OVERFLOW -- size for trigger out of memory
 *     during redo log extension
 */
#define REDO_OVERFLOW ((size_t)((LANE_REDO_EXTERNAL_SIZE\
		/ TX_INTENT_LOG_ENTRY_OVERHEAD) + 1))

static int nobj;

/*
 * free_pool -- frees the pool from all allocated objects
 */
static void
free_pool(PMEMoid *oid)
{
	for (int i = 0; i < nobj; i++) {
		pmemobj_free(&oid[i]);
		UT_ASSERT(OID_IS_NULL(oid[i]));
	}
}

/*
 * fill_pool -- fills the pool with maximum amount of objects.
 *		It tries to fill the pool with object with size of MB.
 *		When it fails, it divides size of the object by DIVIDER,
 *		and tries to fill the pool with object with new smaller size.
 */
static void
fill_pool(PMEMobjpool *pop, PMEMoid *oid)
{
	nobj = 0;
	int ret;
	/* alloc as much space as possible */
	for (size_t size = MB; size >= MIN_ALLOC; size /= DIVIDER) {
		ret = 0;
		while (ret == 0 && nobj < MAX_OBJECTS) {
			ret = pmemobj_alloc(pop, &oid[nobj], size,
				0, NULL, NULL);
			if (!ret)
				nobj++;
		}
	}
}

/*
 * do_tx_max_alloc_tx_publish_abort -- fills the pool and then tries
 *		to overfill redo log - transaction abort expected
 */
static void
do_tx_max_alloc_tx_publish_abort(PMEMobjpool *pop)
{
	UT_OUT("do_tx_max_alloc_tx_publish_abort");
	PMEMoid oid[MAX_OBJECTS];
	PMEMoid oid2[REDO_OVERFLOW];
	struct pobj_action act[REDO_OVERFLOW];

	for (int i = 0; i < REDO_OVERFLOW; i++) {
		/* size is 64 - it can be any size */
		oid2[i] = pmemobj_reserve(pop, &act[i], 64, 0);
		UT_ASSERT(!OID_IS_NULL(oid2[i]));
	}

	fill_pool(pop, oid);

	/* it should abort - cannot extend redo log */
	TX_BEGIN(pop) {
		pmemobj_tx_publish(act, REDO_OVERFLOW);
	} TX_ONABORT {
		UT_OUT("!Cannot publish");
	} TX_ONCOMMIT {
		UT_FATAL("Can publish");
	} TX_END

	free_pool(oid);
	pmemobj_cancel(pop, act, REDO_OVERFLOW);
}

static void
do_ctl_overhead_test(PMEMobjpool *pop)
{
	int ret;
	unsigned ro_value = 0;

	ret = pmemobj_ctl_get(pop, "tx.snapshot.alignment", &ro_value);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(ro_value, TX_SNAPSHOT_ALIGNMENT);

	ro_value = 0;
	ret = pmemobj_ctl_get(pop, "tx.intent.alignment", &ro_value);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(ro_value, TX_INTENT_ALIGNMENT);

	ro_value = 0;
	ret = pmemobj_ctl_get(pop, "tx.snapshot.log.entry_overhead", &ro_value);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(ro_value, TX_SNAPSHOT_LOG_ENTRY_OVERHEAD);

	ro_value = 0;
	ret = pmemobj_ctl_get(pop, "tx.intent.log.entry_overhead", &ro_value);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(ro_value, TX_INTENT_LOG_ENTRY_OVERHEAD);

	ro_value = 0;
	ret = pmemobj_ctl_get(pop, "tx.snapshot.log.buffer_overhead",
			&ro_value);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(ro_value, TX_SNAPSHOT_LOG_BUFFER_OVERHEAD);

	ro_value = 0;
	ret = pmemobj_ctl_get(pop, "tx.intent.log.buffer_overhead", &ro_value);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(ro_value, TX_INTENT_LOG_BUFFER_OVERHEAD);
}

static void
do_tx_max_alloc_no_prealloc_snap(PMEMobjpool *pop)
{
	UT_OUT("no_prealloc_snap");
	PMEMoid oid[MAX_OBJECTS];

	fill_pool(pop, oid);

	/* pool is full now, so let's try to snapshot first object */
	size_t snap_size = pmemobj_alloc_usable_size(oid[0]);
	TX_BEGIN(pop) {
		/* it should abort - cannot allocate memory */
		pmemobj_tx_add_range(oid[0], 0, snap_size);
	} TX_ONABORT {
		UT_OUT("!Cannot add snapshot");
	} TX_ONCOMMIT {
		UT_FATAL("Can add snapshot");
	} TX_END

	free_pool(oid);
}

static void
do_tx_max_alloc_prealloc_snap(PMEMobjpool *pop)
{
	UT_OUT("prealloc_snap");
	PMEMoid oid[MAX_OBJECTS];

	fill_pool(pop, oid);

	/* pool is full now, so let's try to snapshot first object */
	size_t snap_size = pmemobj_alloc_usable_size(oid[0]);
	void *addr = pmemobj_direct(oid[0]);

	TX_BEGIN(pop) {
		pmemobj_tx_log_append_buffer(
			TX_LOG_TYPE_SNAPSHOT, addr, snap_size);
		pmemobj_tx_add_range(oid[0], 0, snap_size);
	} TX_ONABORT {
		UT_FATAL("!Cannot add snapshot");
	} TX_ONCOMMIT {
		UT_OUT("Can add snapshot");
	} TX_END

	free_pool(oid);
}

static void
do_tx_max_alloc_prealloc_nested(PMEMobjpool *pop)
{
	UT_OUT("prealloc_nested");
	PMEMoid oid[MAX_OBJECTS];

	fill_pool(pop, oid);

	size_t snap_size = pmemobj_alloc_usable_size(oid[1]);
	void *addr = pmemobj_direct(oid[1]);

	TOID(struct object) obj0;
	TOID(struct object) obj1;
	TOID_ASSIGN(obj0, oid[0]);
	TOID_ASSIGN(obj1, oid[1]);

	TX_BEGIN(pop) {
		/* do antything */
		D_RW(obj0)->value = TEST_VALUE_1;
		TX_BEGIN(pop) {
			pmemobj_tx_log_append_buffer(
				TX_LOG_TYPE_SNAPSHOT, addr, snap_size);
			pmemobj_tx_add_range(oid[1], 0, snap_size);
			D_RW(obj1)->value = TEST_VALUE_2;
		} TX_ONABORT {
			UT_FATAL("!Cannot add snapshot");
		} TX_ONCOMMIT {
			UT_OUT("Can add snapshot");
		} TX_END
	} TX_END

	UT_ASSERTeq(D_RO(obj1)->value, TEST_VALUE_2);
	free_pool(oid);
}

static void
do_tx_max_alloc_prealloc_snap_multi(PMEMobjpool *pop)
{
	UT_OUT("prealloc_snap_multi");
	PMEMoid oid[MAX_OBJECTS];

	fill_pool(pop, oid);

	/* pool is full now, so let's try to snapshot first object */
	size_t snap_size0 = pmemobj_alloc_usable_size(oid[0]);
	void *addr0 = pmemobj_direct(oid[0]);
	size_t snap_size1 = pmemobj_alloc_usable_size(oid[1]);
	void *addr1 = pmemobj_direct(oid[1]);
	size_t snap_size2 = pmemobj_alloc_usable_size(oid[2]);
	void *addr2 = pmemobj_direct(oid[2]);

	errno = 0;
	TX_BEGIN(pop) {
		/* abort expected - cannot allocate memory */
		pmemobj_tx_log_append_buffer(
			TX_LOG_TYPE_SNAPSHOT, addr0, snap_size0);
		pmemobj_tx_log_append_buffer(
			TX_LOG_TYPE_SNAPSHOT, addr1, snap_size1);
		pmemobj_tx_log_append_buffer(
			TX_LOG_TYPE_SNAPSHOT, addr2, snap_size2);

		pmemobj_tx_add_range(oid[0], 0, snap_size0);
		pmemobj_tx_add_range(oid[1], 0, snap_size1);
		pmemobj_tx_add_range(oid[2], 0, snap_size2);
	} TX_ONABORT {
		UT_FATAL("!Cannot add snapshot");
	} TX_ONCOMMIT {
		UT_OUT("Can add snapshot");
	} TX_END

	free_pool(oid);
}

static void
do_tx_do_not_auto_reserve_snapshot(PMEMobjpool *pop)
{
	UT_OUT("do_not_auto_reserve");
	PMEMoid oid0, oid1;

	int err = pmemobj_alloc(pop, &oid0, 1024, 0, NULL, NULL);
	UT_ASSERTeq(err, 0);
	err = pmemobj_alloc(pop, &oid1, 1024, 0, NULL, NULL);
	UT_ASSERTeq(err, 0);

	TX_BEGIN(pop) {
		pmemobj_tx_log_auto_alloc(TX_LOG_TYPE_SNAPSHOT, 0);
		pmemobj_tx_add_range(oid0, 0, 1024);
		/* it should abort - cannot extend ulog (first entry is full) */
		pmemobj_tx_add_range(oid1, 0, 1024);
	} TX_ONABORT {
		UT_OUT("!Cannot reserve more");
	} TX_ONCOMMIT {
		UT_FATAL("Can add snapshot");
	} TX_END

	pmemobj_free(&oid0);
	pmemobj_free(&oid1);
}

/*
 * do_tx_max_alloc_wrong_pop_addr -- allocates two pools and tries to
 * do transaction with the first pool and address from the second pool.
 * Abort expected - cannot allocate from different pool.
 */
static void
do_tx_max_alloc_wrong_pop_addr(PMEMobjpool *pop, PMEMobjpool *pop2)
{
	UT_OUT("wrong_pop_addr");
	PMEMoid oid[MAX_OBJECTS];
	PMEMoid oid2;

	fill_pool(pop, oid);
	pmemobj_alloc(pop2, &oid2, MB, 0, NULL, NULL);

	/* pools are allocated now, let's try to get address from wrong pool */
	size_t snap_size = pmemobj_alloc_usable_size(oid2);
	void *addr2 = pmemobj_direct(oid2);

	/* abort expected - cannot allocate from different pool */
	TX_BEGIN(pop) {
		pmemobj_tx_log_append_buffer(
			TX_LOG_TYPE_SNAPSHOT, addr2, snap_size);
		pmemobj_tx_add_range(oid[0], 0, snap_size);
	} TX_ONABORT {
		UT_OUT("!Allocation from different pool");
	} TX_ONCOMMIT {
		UT_FATAL("Can add snapshot");
	} TX_END

	free_pool(oid);
	pmemobj_free(&oid2);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_ulog_size");

	if (argc != 3)
		UT_FATAL("usage: %s [file]", argv[0]);

	PMEMobjpool *pop = pmemobj_create(argv[1], LAYOUT_NAME, 0,
		S_IWUSR | S_IRUSR);

	if (pop == NULL)
		UT_FATAL("!pmemobj_create");

	PMEMobjpool *pop2 = pmemobj_create(argv[2], LAYOUT_NAME, 0,
		S_IWUSR | S_IRUSR);

	if (pop2 == NULL)
		UT_FATAL("!pmemobj_create");

	do_ctl_overhead_test(pop);
	do_tx_max_alloc_no_prealloc_snap(pop);
	do_tx_max_alloc_prealloc_snap(pop);
	do_tx_max_alloc_prealloc_nested(pop);
	do_tx_max_alloc_prealloc_snap_multi(pop);
	do_tx_do_not_auto_reserve_snapshot(pop);
	do_tx_max_alloc_wrong_pop_addr(pop, pop2);
	do_tx_max_alloc_tx_publish_abort(pop);

	pmemobj_close(pop);
	pmemobj_close(pop2);

	DONE(NULL);
}

#ifdef _MSC_VER
extern "C" {
	/*
	 * Since libpmemobj is linked statically,
	 * we need to invoke its ctor/dtor.
	 */
	MSVC_CONSTR(libpmemobj_init)
	MSVC_DESTR(libpmemobj_fini)
}
#endif
