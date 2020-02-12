// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019, Intel Corporation */

/*
 * obj_ulog_size.c -- unit tests for pmemobj_action API and
 * redo, undo logs
 */
#include <sys/param.h>
#include <string.h>
#include <stddef.h>

#include "unittest.h"

#include "util.h"
/*
 * tx.h -- needed for TX_SNAPSHOT_LOG_ENTRY_ALIGNMENT,
 * TX_SNAPSHOT_LOG_BUFFER_OVERHEAD, TX_SNAPSHOT_LOG_ENTRY_OVERHEAD,
 * TX_INTENT_LOG_BUFFER_ALIGNMENT, TX_INTENT_LOG_BUFFER_OVERHEAD,
 * TX_INTENT_LOG_ENTRY_OVERHEAD
 */
#include "tx.h"

/* needed for LANE_REDO_EXTERNAL_SIZE and LANE_UNDO_SIZE */
#include "lane.h"

#define LAYOUT_NAME "obj_ulog_size"

#define MIN_ALLOC 64
#define MAX_ALLOC (1024 * 1024)
#define HALF_OF_DEFAULT_UNDO_SIZE (LANE_UNDO_SIZE / 2)
#define ARRAY_SIZE_COMMON 3

/* the ranges of indices are describing the use of some allocations */
#define LOG_BUFFER 0
#define LOG_BUFFER_NUM 6
#define RANGE (LOG_BUFFER + LOG_BUFFER_NUM)
#define RANGE_NUM 6
#define MIN_NOIDS (RANGE + RANGE_NUM)

/*
 * REDO_OVERFLOW -- size for trigger out of memory
 * during redo log extension
 */
#define REDO_OVERFLOW ((size_t)((LANE_REDO_EXTERNAL_SIZE\
		/ TX_INTENT_LOG_ENTRY_OVERHEAD) + 1))

#define APPEND_SIZE SIZEOF_ALIGNED_ULOG(CACHELINE_SIZE)

/*
 * free_pool -- frees the pool from all allocated objects
 * and releases oids dynamic array
 */
static void
free_pool(PMEMoid *oids, size_t noids)
{
	for (size_t i = 0; i < noids; i++) {
		pmemobj_free(&oids[i]);
		UT_ASSERT(OID_IS_NULL(oids[i]));
	}

	FREE(oids);
}

/*
 * fill_pool -- fills provided pmemobj pool with as many allocations
 * as possible. Returns array of PMEMoids allocated from the
 * provided pool. The number of valid allocation stored in the
 * returned array is stored in the noids output argument.
 */
static PMEMoid *
fill_pool(PMEMobjpool *pop, size_t *noids)
{
	size_t oids_size = 2048; /* let's start with something big enough */
	PMEMoid *oids = (PMEMoid *)MALLOC(oids_size * sizeof(PMEMoid));

	*noids = 0;
	int ret;
	/* alloc as much space as possible */
	for (size_t size = MAX_ALLOC; size >= MIN_ALLOC; size /= 2) {
		ret = 0;

		while (ret == 0) {
			ret = pmemobj_alloc(pop, &oids[*noids], size,
				0, NULL, NULL);
			if (!ret)
				(*noids)++;
			if (*noids == oids_size) {
				oids_size *= 2;
				oids = (PMEMoid *)REALLOC(oids, oids_size *
					sizeof(PMEMoid));
			}
		}
	}
	return oids;
}

/*
 * do_tx_max_alloc_tx_publish_abort -- fills the pool and then tries
 * to overfill redo log - transaction abort expected
 */
static void
do_tx_max_alloc_tx_publish_abort(PMEMobjpool *pop)
{
	UT_OUT("do_tx_max_alloc_tx_publish_abort");
	PMEMoid *allocated = NULL;
	PMEMoid reservations[REDO_OVERFLOW];
	size_t nallocated = 0;
	struct pobj_action act[REDO_OVERFLOW];

	for (int i = 0; i < REDO_OVERFLOW; i++) {
		reservations[i] = pmemobj_reserve(pop, &act[i], MIN_ALLOC, 0);
		UT_ASSERT(!OID_IS_NULL(reservations[i]));
	}

	allocated = fill_pool(pop, &nallocated);
	/*
	 * number of allocated buffers is not important
	 * they are not used anyway
	 */

	/* it should abort - cannot extend redo log */
	TX_BEGIN(pop) {
		pmemobj_tx_publish(act, REDO_OVERFLOW);
	} TX_ONABORT {
		UT_OUT("!Cannot extend redo log - the pool is full");
	} TX_ONCOMMIT {
		UT_FATAL("Can extend redo log despite the pool is full");
	} TX_END

	/* it should fail without abort transaction */
	TX_BEGIN(pop) {
		pmemobj_tx_xpublish(act, REDO_OVERFLOW, POBJ_XPUBLISH_NO_ABORT);
	} TX_ONABORT {
		ASSERT(0);
	} TX_ONCOMMIT {
		UT_ASSERTeq(errno, ENOMEM);
		UT_OUT("!Cannot extend redo log - the pool is full");
	} TX_END

	free_pool(allocated, nallocated);
	pmemobj_cancel(pop, act, REDO_OVERFLOW);
}

/*
 * do_tx_max_alloc_no_user_alloc_snap -- fills the pool and tries to do
 * snapshot which is bigger than ulog size
 */
static void
do_tx_max_alloc_no_user_alloc_snap(PMEMobjpool *pop)
{
	UT_OUT("do_tx_max_alloc_no_user_alloc_snap");
	size_t nallocated = 0;
	PMEMoid *allocated = fill_pool(pop, &nallocated);
	UT_ASSERT(nallocated >= MIN_NOIDS);

	size_t range_size = pmemobj_alloc_usable_size(allocated[LOG_BUFFER]);
	UT_ASSERT(range_size > LANE_UNDO_SIZE);

	void *range_addr = pmemobj_direct(allocated[LOG_BUFFER]);
	pmemobj_memset(pop, range_addr, 0, range_size, 0);

	TX_BEGIN(pop) {
		/* it should abort - cannot extend undo log */
		pmemobj_tx_add_range(allocated[LOG_BUFFER], 0, range_size);
	} TX_ONABORT {
		UT_OUT("!Cannot extend undo log - the pool is full");
	} TX_ONCOMMIT {
		UT_FATAL("Can extend undo log despite the pool is full");
	} TX_END

	free_pool(allocated, nallocated);
}

/*
 * do_tx_max_alloc_user_alloc_snap -- fills the pool, appends allocated
 * buffer and tries to do snapshot which is bigger than ulog size
 */
static void
do_tx_max_alloc_user_alloc_snap(PMEMobjpool *pop)
{
	UT_OUT("do_tx_max_alloc_user_alloc_snap");
	size_t nallocated = 0;
	PMEMoid *allocated = fill_pool(pop, &nallocated);
	UT_ASSERT(nallocated >= MIN_NOIDS);

	size_t buff_size = pmemobj_alloc_usable_size(allocated[LOG_BUFFER]);
	void *buff_addr = pmemobj_direct(allocated[LOG_BUFFER]);
	size_t range_size = pmemobj_alloc_usable_size(allocated[RANGE]);
	UT_ASSERT(range_size > LANE_UNDO_SIZE);

	void *range_addr = pmemobj_direct(allocated[RANGE]);
	pmemobj_memset(pop, range_addr, 0, range_size, 0);

	TX_BEGIN(pop) {
		pmemobj_tx_log_append_buffer(
			TX_LOG_TYPE_SNAPSHOT, buff_addr, buff_size);
		pmemobj_tx_add_range(allocated[RANGE], 0, range_size);
	} TX_ONABORT {
		UT_FATAL("!Cannot use the user appended undo log buffer");
	} TX_ONCOMMIT {
		UT_OUT("Can use the user appended undo log buffer");
	} TX_END

	free_pool(allocated, nallocated);
}

/*
 * do_tx_max_alloc_user_alloc_nested -- example of buffer appending
 * allocated by the user in a nested transaction
 */
static void
do_tx_max_alloc_user_alloc_nested(PMEMobjpool *pop)
{
	UT_OUT("do_tx_max_alloc_user_alloc_nested");
	size_t nallocated = 0;
	PMEMoid *allocated = fill_pool(pop, &nallocated);
	UT_ASSERT(nallocated >= MIN_NOIDS);

	size_t buff_size = pmemobj_alloc_usable_size(allocated[LOG_BUFFER]);
	void *buff_addr = pmemobj_direct(allocated[LOG_BUFFER]);
	size_t range_size = pmemobj_alloc_usable_size(allocated[RANGE]);

	void *range_addr = pmemobj_direct(allocated[RANGE]);
	pmemobj_memset(pop, range_addr, 0, range_size, 0);

	TX_BEGIN(pop) {
		TX_BEGIN(pop) {
			pmemobj_tx_log_append_buffer(
				TX_LOG_TYPE_SNAPSHOT, buff_addr, buff_size);
			pmemobj_tx_add_range(allocated[RANGE], 0, range_size);
		} TX_ONABORT {
			UT_FATAL(
				"Cannot use the undo log appended by the user in a nested transaction");
		} TX_ONCOMMIT {
			UT_OUT(
				"Can use the undo log appended by the user in a nested transaction");
		} TX_END
	} TX_END

	free_pool(allocated, nallocated);
}

/*
 * do_tx_max_alloc_user_alloc_snap_multi -- appending of many buffers
 * in one transaction
 */
static void
do_tx_max_alloc_user_alloc_snap_multi(PMEMobjpool *pop)
{
	UT_OUT("do_tx_max_alloc_user_alloc_snap_multi");
	size_t nallocated = 0;
	PMEMoid *allocated = fill_pool(pop, &nallocated);
	UT_ASSERT(nallocated >= MIN_NOIDS);

	size_t buff_sizes[ARRAY_SIZE_COMMON];
	void *buff_addrs[ARRAY_SIZE_COMMON];
	size_t range_sizes[ARRAY_SIZE_COMMON];
	void *range_addrs[ARRAY_SIZE_COMMON];

	/*
	 * The maximum value of offset used in the for-loop below is
	 * i_max == (ARRAY_SIZE_COMMON - 1) * 2.
	 * It will cause using LOG_BUFFER + i_max and RANGE + i_max indices so
	 * i_max has to be less than LOG_BUFFER_NUM and
	 * i_max has to be less than RANGE_NUM.
	 */
	UT_COMPILE_ERROR_ON((ARRAY_SIZE_COMMON - 1) * 2 >= LOG_BUFFER_NUM);
	UT_COMPILE_ERROR_ON((ARRAY_SIZE_COMMON - 1) * 2 >= RANGE_NUM);

	for (unsigned long i = 0; i < ARRAY_SIZE_COMMON; i++) {
		/* we multiply the value to not use continuous memory blocks */
		buff_sizes[i] = pmemobj_alloc_usable_size(
			allocated[LOG_BUFFER + (i *2)]);
		buff_addrs[i] = pmemobj_direct(
			allocated[LOG_BUFFER + (i * 2)]);
		range_sizes[i] = pmemobj_alloc_usable_size(
			allocated[RANGE + (i * 2)]);
		range_addrs[i] = pmemobj_direct(allocated[RANGE + (i * 2)]);

		pmemobj_memset(pop, range_addrs[i], 0, range_sizes[i], 0);
	}

	errno = 0;
	TX_BEGIN(pop) {
		pmemobj_tx_log_append_buffer(
			TX_LOG_TYPE_SNAPSHOT, buff_addrs[0], buff_sizes[0]);
		pmemobj_tx_log_append_buffer(
			TX_LOG_TYPE_SNAPSHOT, buff_addrs[1], buff_sizes[1]);
		pmemobj_tx_log_append_buffer(
			TX_LOG_TYPE_SNAPSHOT, buff_addrs[2], buff_sizes[1]);

		for (unsigned long i = 0; i < ARRAY_SIZE_COMMON; i++) {
			pmemobj_tx_add_range(allocated[RANGE + (i * 2)], 0,
			range_sizes[i]);
		}
	} TX_ONABORT {
		UT_FATAL("!Cannot use multiple user appended undo log buffers");
	} TX_ONCOMMIT {
		UT_OUT("Can use multiple user appended undo log buffers");
	} TX_END

	/* check if all user allocated buffers are used */
	errno = 0;
	TX_BEGIN(pop) {
		pmemobj_tx_log_append_buffer(
			TX_LOG_TYPE_SNAPSHOT, buff_addrs[0], buff_sizes[0]);
		pmemobj_tx_log_append_buffer(
			TX_LOG_TYPE_SNAPSHOT, buff_addrs[1], buff_sizes[1]);
		/*
		 * do not append last buffer to make sure it is needed for this
		 * transaction to succeed
		 */
		pmemobj_tx_add_range(allocated[RANGE + 0], 0, range_sizes[0]);
		pmemobj_tx_add_range(allocated[RANGE + 2], 0, range_sizes[1]);
		pmemobj_tx_add_range(allocated[RANGE + 4], 0, range_sizes[2]);
	} TX_ONABORT {
		UT_OUT("!All user appended undo log buffers are used");
	} TX_ONCOMMIT {
		UT_FATAL(
			"Not all user appended undo log buffers are required - too small ranges");
	} TX_END

	free_pool(allocated, nallocated);
}

/*
 * do_tx_auto_alloc_disabled -- blocking of automatic expansion
 * of ulog. When auto expansion of ulog is off, snapshot with size
 * of default undo log is going to fail, because of buffer overhead
 * (size of internal undo log and header size).
 */
static void
do_tx_auto_alloc_disabled(PMEMobjpool *pop)
{
	UT_OUT("do_tx_auto_alloc_disabled");
	PMEMoid oid0, oid1;

	int ret = pmemobj_zalloc(pop, &oid0, HALF_OF_DEFAULT_UNDO_SIZE, 0);
	UT_ASSERTeq(ret, 0);
	ret = pmemobj_zalloc(pop, &oid1, HALF_OF_DEFAULT_UNDO_SIZE, 0);
	UT_ASSERTeq(ret, 0);

	TX_BEGIN(pop) {
		pmemobj_tx_log_auto_alloc(TX_LOG_TYPE_SNAPSHOT, 0);
		pmemobj_tx_add_range(oid0, 0, HALF_OF_DEFAULT_UNDO_SIZE);
		/* it should abort - cannot extend ulog (first entry is full) */
		pmemobj_tx_add_range(oid1, 0, HALF_OF_DEFAULT_UNDO_SIZE);
	} TX_ONABORT {
		UT_OUT("!Disabled auto alloc prevented the undo log grow");
	} TX_ONCOMMIT {
		UT_FATAL(
			"Disabled auto alloc did not prevent the undo log grow");
	} TX_END

	pmemobj_free(&oid0);
	pmemobj_free(&oid1);
}

/*
 * do_tx_max_alloc_wrong_pop_addr -- allocates two pools and tries to
 * do transaction with the first pool and address from the second
 * pool. Abort expected - cannot allocate from different pool.
 */
static void
do_tx_max_alloc_wrong_pop_addr(PMEMobjpool *pop, PMEMobjpool *pop2)
{
	UT_OUT("do_tx_max_alloc_wrong_pop_addr");
	size_t nallocated = 0;
	PMEMoid *allocated = fill_pool(pop, &nallocated);
	/*
	 * number of allocated buffers is not important
	 * they are not used anyway
	 */

	PMEMoid oid2;
	int ret = pmemobj_alloc(pop2, &oid2, MAX_ALLOC, 0, NULL, NULL);
	UT_ASSERTeq(ret, 0);

	/* pools are allocated now, let's try to get address from wrong pool */
	size_t buff2_size = pmemobj_alloc_usable_size(oid2);
	void *buff2_addr = pmemobj_direct(oid2);

	/* abort expected - cannot allocate from different pool */
	TX_BEGIN(pop) {
		pmemobj_tx_log_append_buffer(
			TX_LOG_TYPE_SNAPSHOT, buff2_addr, buff2_size);
	} TX_ONABORT {
		UT_OUT(
			"!Cannot append an undo log buffer from a different memory pool");
	} TX_ONCOMMIT {
		UT_FATAL(
			"Can append an undo log buffer from a different memory pool");
	} TX_END

	/* it should fail without abort transaction */
	TX_BEGIN(pop) {
		pmemobj_tx_xlog_append_buffer(TX_LOG_TYPE_SNAPSHOT, buff2_addr,
				buff2_size, POBJ_XLOG_APPEND_BUFFER_NO_ABORT);
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_ONCOMMIT {
		UT_ASSERTeq(errno, EINVAL);
		UT_OUT(
			"!Cannot append an undo log buffer from a different memory pool");
	} TX_END

	free_pool(allocated, nallocated);
	pmemobj_free(&oid2);
}

/*
 * do_tx_buffer_currently_used -- the same buffer cannot be used
 * twice in the same time.
 */
static void
do_tx_buffer_currently_used(PMEMobjpool *pop)
{
	UT_OUT("do_tx_buffer_currently_used");

	PMEMoid oid_buff;
	int verify_user_buffers = 1;

	/* by default verify_user_buffers should be 0 */
	int ret = pmemobj_ctl_get(pop, "tx.debug.verify_user_buffers",
					&verify_user_buffers);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(verify_user_buffers, 0);

	int err = pmemobj_alloc(pop, &oid_buff, MAX_ALLOC, 0, NULL, NULL);
	UT_ASSERTeq(err, 0);
	/* this buffer we will try to use twice */
	size_t buff_size = pmemobj_alloc_usable_size(oid_buff);
	void *buff_addr = pmemobj_direct(oid_buff);

	/* changes verify_user_buffers value */
	verify_user_buffers = 1;
	ret = pmemobj_ctl_set(pop, "tx.debug.verify_user_buffers",
			&verify_user_buffers);
	UT_ASSERTeq(ret, 0);

	verify_user_buffers = 99;
	/* check if verify_user_buffers has changed */
	ret = pmemobj_ctl_get(pop, "tx.debug.verify_user_buffers",
					&verify_user_buffers);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(verify_user_buffers, 1);

	/* if verify_user_buffers is set we should abort tx */
	TX_BEGIN(pop) {
		pmemobj_tx_log_append_buffer(
			TX_LOG_TYPE_SNAPSHOT, buff_addr, buff_size);
		pmemobj_tx_log_append_buffer(
			TX_LOG_TYPE_SNAPSHOT, buff_addr, buff_size);
	} TX_ONABORT {
		UT_OUT("!User cannot append the same undo log buffer twice");
	} TX_ONCOMMIT {
		UT_FATAL("User can append the same undo log buffer twice");
	} TX_END

	pmemobj_free(&oid_buff);

	/* restore the default and verify */
	verify_user_buffers = 0;
	ret = pmemobj_ctl_set(pop, "tx.debug.verify_user_buffers",
			&verify_user_buffers);
	UT_ASSERTeq(ret, 0);
	verify_user_buffers = 99;
	ret = pmemobj_ctl_get(pop, "tx.debug.verify_user_buffers",
					&verify_user_buffers);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(verify_user_buffers, 0);

}

/*
 * do_tx_max_alloc_tx_publish -- fills the pool and then tries
 * to overfill redo log with appended buffer
 */
static void
do_tx_max_alloc_tx_publish(PMEMobjpool *pop)
{
	UT_OUT("do_tx_max_alloc_tx_publish");
	PMEMoid *allocated = NULL;
	PMEMoid reservations[REDO_OVERFLOW];
	size_t nallocated = 0;
	struct pobj_action act[REDO_OVERFLOW];

	for (int i = 0; i < REDO_OVERFLOW; i++) {
		reservations[i] = pmemobj_reserve(pop, &act[i], MIN_ALLOC, 0);
		UT_ASSERT(!OID_IS_NULL(reservations[i]));
	}

	allocated = fill_pool(pop, &nallocated);
	UT_ASSERT(nallocated >= MIN_NOIDS);

	size_t buff_size = pmemobj_alloc_usable_size(allocated[LOG_BUFFER]);
	void *buff_addr = pmemobj_direct(allocated[LOG_BUFFER]);

	TX_BEGIN(pop) {
		pmemobj_tx_log_append_buffer(
			TX_LOG_TYPE_INTENT, buff_addr, buff_size);
		pmemobj_tx_publish(act, REDO_OVERFLOW);
	} TX_ONABORT {
		UT_FATAL("!Cannot extend redo log despite appended buffer");
	} TX_ONCOMMIT {
		UT_OUT("Can extend redo log with appended buffer");
	} TX_END

	free_pool(allocated, nallocated);

	for (int i = 0; i < REDO_OVERFLOW; ++i) {
		pmemobj_free(&reservations[i]);
	}
}

/*
 * do_tx_user_buffer_atomic_alloc -- checks if finish of atomic
 * allocation inside transaction will not break state of the ulog
 * with appended user buffer
 */
static void
do_tx_user_buffer_atomic_alloc(PMEMobjpool *pop)
{
	UT_OUT("do_tx_user_buffer_atomic_alloc");

	PMEMoid user_buffer_oid;
	PMEMoid atomic_alloc_oid;
	PMEMoid reservations[REDO_OVERFLOW];
	struct pobj_action act[REDO_OVERFLOW];
	/*
	 * we have to fill out first ulog in the redo log
	 * to make sure that the user buffer will be needed
	 * to proceed
	 */
	for (int i = 0; i < REDO_OVERFLOW; i++) {
		reservations[i] = pmemobj_reserve(pop, &act[i], MIN_ALLOC, 0);
		UT_ASSERT(!OID_IS_NULL(reservations[i]));
	}

	/* allocs some space for intent user buffer */
	int ret = pmemobj_alloc(pop, &user_buffer_oid, MAX_ALLOC,
				0, NULL, NULL);
	UT_ASSERTeq(ret, 0);

	size_t buff_size = pmemobj_alloc_usable_size(user_buffer_oid);
	void *buff_addr = pmemobj_direct(user_buffer_oid);

	TX_BEGIN(pop) {
		/* disable automatic ulog reservation and add user buffer */
		pmemobj_tx_log_auto_alloc(TX_LOG_TYPE_INTENT, 0);
		pmemobj_tx_log_append_buffer(TX_LOG_TYPE_INTENT,
				buff_addr, buff_size);

		/* perform atomic allocation in the middle of transaction */
		pmemobj_alloc(pop, &atomic_alloc_oid, MAX_ALLOC,
			0, NULL, NULL);

		/* user buffer should be sill valid, so we try to use it */
		pmemobj_tx_publish(act, REDO_OVERFLOW);
	} TX_ONCOMMIT {
		UT_OUT(
			"The transaction state is consistent after atomic allocation");
	} TX_ONABORT {
		UT_FATAL(
			"The transaction state is consistent after atomic allocation");
	} TX_END

	pmemobj_free(&user_buffer_oid);
}

/*
 * do_tx_buffer_overlapping -- checks if user buffer overlap detection works
 */
static void
do_tx_buffer_overlapping(PMEMobjpool *pop)
{
	UT_OUT("do_tx_buffer_overlapping");

	/* changes verify_user_buffers value */
	int verify_user_buffers = 1;
	int ret = pmemobj_ctl_set(pop, "tx.debug.verify_user_buffers",
			&verify_user_buffers);
	UT_ASSERTeq(ret, 0);

	PMEMoid oid = OID_NULL;
	pmemobj_alloc(pop, &oid, MAX_ALLOC, 0, NULL, NULL);
	UT_ASSERT(!OID_IS_NULL(oid));

	char *ptr = (char *)pmemobj_direct(oid);
	ptr = (char *)ALIGN_UP((size_t)ptr, CACHELINE_SIZE);

	TX_BEGIN(pop) {
		pmemobj_tx_log_append_buffer(TX_LOG_TYPE_INTENT,
			ptr + APPEND_SIZE, APPEND_SIZE);
		pmemobj_tx_log_append_buffer(TX_LOG_TYPE_INTENT,
			ptr, APPEND_SIZE);
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_ONCOMMIT {
		UT_OUT("Overlap not detected");
	} TX_END

	TX_BEGIN(pop) {
		pmemobj_tx_log_append_buffer(TX_LOG_TYPE_INTENT,
			ptr, APPEND_SIZE);
		pmemobj_tx_log_append_buffer(TX_LOG_TYPE_INTENT,
			ptr + APPEND_SIZE, APPEND_SIZE);
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_ONCOMMIT {
		UT_OUT("Overlap not detected");
	} TX_END

	TX_BEGIN(pop) {
		pmemobj_tx_log_append_buffer(TX_LOG_TYPE_INTENT,
			ptr, APPEND_SIZE);
		pmemobj_tx_log_append_buffer(TX_LOG_TYPE_INTENT,
			ptr, APPEND_SIZE);
	} TX_ONABORT {
		UT_OUT("Overlap detected");
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	TX_BEGIN(pop) {
		pmemobj_tx_log_append_buffer(TX_LOG_TYPE_INTENT,
			ptr, APPEND_SIZE);
		pmemobj_tx_log_append_buffer(TX_LOG_TYPE_INTENT,
			ptr + 128, APPEND_SIZE);
	} TX_ONABORT {
		UT_OUT("Overlap detected");
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	TX_BEGIN(pop) {
		pmemobj_tx_log_append_buffer(TX_LOG_TYPE_INTENT,
			ptr + 128, APPEND_SIZE);
		pmemobj_tx_log_append_buffer(TX_LOG_TYPE_INTENT,
			ptr, APPEND_SIZE);
	} TX_ONABORT {
		UT_OUT("Overlap detected");
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	pmemobj_free(&oid);
	verify_user_buffers = 0;
	ret = pmemobj_ctl_set(pop, "tx.debug.verify_user_buffers",
		&verify_user_buffers);
	UT_ASSERTeq(ret, 0);
}

/*
 * do_log_intents_max_size_limits -- test the pmemobj_tx_log_intents_max_size
 * function argument processing
 */
static void
do_log_intents_max_size_limits(void)
{
	UT_OUT("do_log_intent_max_size_limits");

	size_t size = 0;

	/* 1st case */
	size = pmemobj_tx_log_intents_max_size(0);
	UT_ASSERT(size > 0);
	UT_ASSERTne(size, SIZE_MAX);

	/* 2nd case */
	size = pmemobj_tx_log_intents_max_size(
		SIZE_MAX / TX_INTENT_LOG_ENTRY_OVERHEAD);
	UT_ASSERTeq(size, SIZE_MAX);
	UT_ASSERTeq(errno, ERANGE);

	/* 3rd case */
	const size_t toobign =
			(SIZE_MAX - TX_INTENT_LOG_BUFFER_OVERHEAD)
			/ TX_INTENT_LOG_ENTRY_OVERHEAD + 1;
	size = pmemobj_tx_log_intents_max_size(toobign);
	UT_ASSERTeq(size, SIZE_MAX);
	UT_ASSERTeq(errno, ERANGE);
}

/*
 * do_log_intents_max_size -- verify pmemobj_tx_log_intents_max_size reported
 * size is sufficient
 */
static void
do_log_intents_max_size(PMEMobjpool *pop)
{
	UT_OUT("do_log_intent_max_size");

	const size_t nintents = 15; /* an arbitrarily picked number */

	/* query a required log size */
	size_t req_buff_size = pmemobj_tx_log_intents_max_size(nintents);
	UT_ASSERTne(req_buff_size, SIZE_MAX);

	/* alloc the intent buffer */
	PMEMoid buff_oid = OID_NULL;
	int ret = pmemobj_alloc(pop, &buff_oid, req_buff_size, 0, NULL, NULL);
	UT_ASSERTeq(ret, 0);
	void *buff_addr = pmemobj_direct(buff_oid);
	size_t buff_size = pmemobj_alloc_usable_size(buff_oid);
	UT_ASSERT(buff_size >= req_buff_size);

	/* make an assumed number of reservations */
	PMEMoid reservations[nintents];
	struct pobj_action act[nintents];
	for (size_t i = 0; i < nintents; i++) {
		reservations[i] = pmemobj_reserve(pop, &act[i], MIN_ALLOC, 0);
		UT_ASSERT(!OID_IS_NULL(reservations[i]));
	}

	TX_BEGIN(pop) {
		pmemobj_tx_log_auto_alloc(TX_LOG_TYPE_INTENT, 0);
		pmemobj_tx_log_append_buffer(
			TX_LOG_TYPE_INTENT, buff_addr, buff_size);
		pmemobj_tx_publish(act, nintents);
	} TX_ONABORT {
		UT_FATAL("!Estimated intent log buffer size is too small");
	} TX_ONCOMMIT {
		UT_OUT("Estimated intent log buffer size is sufficient");
	} TX_END

	/* release all allocated resources */
	for (size_t i = 0; i < nintents; ++i) {
		pmemobj_free(&reservations[i]);
		UT_ASSERT(OID_IS_NULL(reservations[i]));
	}
	pmemobj_free(&buff_oid);
	UT_ASSERT(OID_IS_NULL(buff_oid));
}

/*
 * do_log_snapshots_max_size_limits -- test the
 * pmemobj_tx_log_snapshots_max_size function argument processing
 */
static void
do_log_snapshots_max_size_limits(void)
{
	UT_OUT("do_log_snapshot_max_size_limits");

	const size_t nsizes = 1024; /* an arbitrarily picked number */

	/* prepare array of big sizes */
	size_t *sizes = (size_t *)MALLOC(sizeof(size_t) * nsizes);
	for (size_t i = 0, size = MAX_ALLOC; i < nsizes; ++i) {
		sizes[i] = size;

		if (size < SIZE_MAX / 2)
			size *= 2;
	}

	size_t size = 0;

	size = pmemobj_tx_log_snapshots_max_size(sizes, nsizes);
	UT_ASSERTeq(size, SIZE_MAX);
	UT_ASSERTeq(errno, ERANGE);

	/* release allocated resources */
	FREE(sizes);
}

/*
 * do_log_snapshots_max_size -- verify pmemobj_tx_log_snapshots_max_size
 * reported size is sufficient
 */
static void
do_log_snapshots_max_size(PMEMobjpool *pop)
{
	UT_OUT("do_log_snapshot_max_size");

	size_t nsizes_max = 15; /* an arbitrarily picked number */
	size_t *sizes = (size_t *)MALLOC(nsizes_max * sizeof(size_t));

	/* fill up the pool */
	size_t nallocated = 0;
	PMEMoid *allocated = fill_pool(pop, &nallocated);
	UT_ASSERT(nallocated > LOG_BUFFER);

	/* the first allocation will be used for as a snapshot log buffer */
	void *buff_addr = pmemobj_direct(allocated[LOG_BUFFER]);
	size_t max_buff_size = pmemobj_alloc_usable_size(allocated[LOG_BUFFER]);
	size_t req_buff_size = 0;

	/* how many ranges fit into the buffer */
	size_t nsizes_valid = 0;
	for (size_t i = nallocated - 1; i > LOG_BUFFER; --i) {
		/* initialize the range */
		size_t range_size = pmemobj_alloc_usable_size(allocated[i]);
		void *range_addr = pmemobj_direct(allocated[i]);
		pmemobj_memset(pop, range_addr, 0, range_size, 0);

		/* append to the list of sizes */
		sizes[nsizes_valid]  = range_size;
		++nsizes_valid;
		if (nsizes_valid == nsizes_max) {
			nsizes_max *= 2;
			sizes = (size_t *)REALLOC(sizes,
					nsizes_max * sizeof(size_t));
		}

		/* estimate a required buffer size for snapshots */
		req_buff_size = pmemobj_tx_log_snapshots_max_size(
				sizes, nsizes_valid);
		UT_ASSERTne(req_buff_size, SIZE_MAX);
		if (req_buff_size > max_buff_size) {
			/* if it is too much we have to use one less */
			--nsizes_valid;
			UT_ASSERTne(nsizes_valid, 0);
			req_buff_size = pmemobj_tx_log_snapshots_max_size(
					sizes, nsizes_valid);
			break;
		}
	}

	TX_BEGIN(pop) {
		pmemobj_tx_log_append_buffer(TX_LOG_TYPE_SNAPSHOT, buff_addr,
				req_buff_size);
		for (size_t i = 0; i < nsizes_valid; i++) {
			pmemobj_tx_add_range(allocated[nallocated - i - 1], 0,
					sizes[i]);
		}
	} TX_ONABORT {
		UT_FATAL("!Estimated snapshot log buffer size is too small");
	} TX_ONCOMMIT {
		UT_OUT("Estimated snapshot log buffer size is sufficient");
	} TX_END

	/* release all allocated resources */
	free_pool(allocated, nallocated);
	FREE(sizes);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_ulog_size");

	if (argc != 3)
		UT_FATAL("usage: %s [file] [file1]", argv[0]);

	PMEMobjpool *pop = pmemobj_create(argv[1], LAYOUT_NAME, 0,
		S_IWUSR | S_IRUSR);

	if (pop == NULL)
		UT_FATAL("!pmemobj_create");

	PMEMobjpool *pop2 = pmemobj_create(argv[2], LAYOUT_NAME, 0,
		S_IWUSR | S_IRUSR);

	if (pop2 == NULL)
		UT_FATAL("!pmemobj_create");

	do_tx_max_alloc_no_user_alloc_snap(pop);
	do_tx_max_alloc_user_alloc_snap(pop);
	do_tx_max_alloc_user_alloc_nested(pop);
	do_tx_max_alloc_user_alloc_snap_multi(pop);
	do_tx_auto_alloc_disabled(pop);
	do_tx_max_alloc_wrong_pop_addr(pop, pop2);
	do_tx_max_alloc_tx_publish_abort(pop);
	do_tx_buffer_currently_used(pop);
	do_tx_max_alloc_tx_publish(pop);
	do_tx_user_buffer_atomic_alloc(pop);
	do_tx_buffer_overlapping(pop);

	do_log_intents_max_size_limits();
	do_log_intents_max_size(pop);
	do_log_snapshots_max_size_limits();
	do_log_snapshots_max_size(pop);

	pmemobj_close(pop);
	pmemobj_close(pop2);

	DONE(NULL);
}
