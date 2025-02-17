// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2024, Intel Corporation */

/*
 * obj_ulog_advanced.c -- a test targetting redo logs of size between a single
 *			  persistent redo log size and the initial size of
 *			  the persistent shadow log
 */

#include <stdbool.h>
#include <stdlib.h>
#include "pmemops.h"
#include "memops.h"
#include "ulog.h"
#include "unittest.h"

#define LAYOUT_NAME "obj_ulog_advanced"

#if VG_PMEMCHECK_ENABLED == 0
#define VALGRIND_PMC_EMIT_LOG(_) /* NOP */
#endif /* VG_PMEMCHECK_ENABLED == 0 */

/*
 * BACKGROUND INFORMATION
 *
 * The persistent redo log is a PMEM buffer where a sequence of operations is
 * stored before processing it. Having it stored persistently ensures either all
 * of the operations or none of them will eventually take place no matter
 * the interruptions.
 *
 * The persistent shadow log is a DRAM buffer where intially all redo log
 * entries are placed. This log's initial capacity is 1KiB (ULOG_BASE_SIZE)
 * and can be reallocated to grow bigger as necessary. If the write offset
 * before adding the new entry + CACHELINE_SIZE (64B) == capacity then
 * the persistent shadow log will grow by ULOG_BASE_SIZE each time.
 *
 * When the user chooses the persistent shadow log to be processed, it will be
 * copied to the persistent redo log before processing.
 *
 * The persistent redo log's maximum capacity is 640B (LANE_REDO_EXTERNAL_SIZE).
 * So, when the persistent shadow log is bigger, additional redo logs
 * have to be allocated and linked to the first one before the persistent
 * shadow log will be copied.
 *
 * The header of the persistent shadow log is of exactly the same structure as
 * the header of the persistent redo log and one of its fields stores
 * the capacity. It turns out to be confusing since it is not obvious whether
 * the persistent shadow log's capacity is the actual capacity of the underlying
 * DRAM buffer (>=1024B) or the capacity of a single persistent redo log
 * (<=640B). There is no good answer to this conundrum since the persistent
 * shadow log actually serves both of these purposes.
 *
 * ISSUE
 *
 * The DAOS developers observed a real issue occurring in their BMEM allocator
 * which is based on PMEMOBJ (daos-stack/daos#11593). The issue occurred when
 * the entries fell above the LANE_REDO_EXTERNAL_SIZE offset but there were
 * not enough entries to trigger the persistent shadow log's growth
 * (<= ULOG_BASE_SIZE - CACHELINE_SIZE = 960B).
 *
 * TEST
 *
 * This test aims at reproducing the issue to ensure it is not present
 * in PMEMOBJ. It is achieved by implementing a few scenarios:
 *
 * 1a. test_init_publish_abort_and_verify - the publishing process is aborted
 *     just after the persistent shadow log is copied to the persistent redo
 *     log. Another process verifies whether the state of the pool is correctly
 *     restored from the persistent redo log.
 * 1b. the 1a but with error injection as described in the issue section.
 *     Please see the ulog_store mock below for details.
 *
 * Both 1a and 1b are run against various numbers of operations accumulated
 * in the persistent shadow log so all of the known cases are hit:
 * - X < LANE_REDO_EXTERNAL_SIZE (1a-only)
 * - X == LANE_REDO_EXTERNAL_SIZE (1a-only)
 * - LANE_REDO_EXTERNAL_SIZE < X < (ULOG_BASE_SIZE - CACHELINE_SIZE)
 * - X == (ULOG_BASE_SIZE - CACHELINE_SIZE)
 * - X > (ULOG_BASE_SIZE - CACHELINE_SIZE)
 *
 * 2a. the stores for test_publish are recorded and the publishing
 *     process is terminated normally. The pmreorder is employed to reorder
 *     the stores and test_verify is called to check the consistency of
 *     the published sequence where the expected number of operations is known.
 *     Please see common_replay_and_check for details.
 * 2b. the 2a but with error injection as described in the issue section.
 *     Please see the ulog_store mock below for details.
 *
 * Due to time constraints 2a and 2b are run only against one hand-picked
 * and considered as critical for the issue at hand number of operations:
 * - X == (ULOG_BASE_SIZE - CACHELINE_SIZE)
 */

/* exit code of a process when terminated in consequence of SIGABRT */
#define SIGABRT_EXITSTATUS 134

/* a single redo log entry's size - true only for a set-operation */
#define ENTRY_SIZE (sizeof(struct ulog_entry_val))

/*
 * A 'slot' for the sake of this test means a single 64b value in persistent
 * memory which set-operations will target.
 *
 * The maximum number of slots in a persistent shadow log before triggering its
 * growth.
 */
#define SLOTS_NUM_MAX_BEFORE_GROWTH \
	((ULOG_BASE_SIZE - CACHELINE_SIZE) / ENTRY_SIZE) /* 60 */
/* The number of set-operations that can fit in a single persistent redo log. */
#define SLOTS_PER_REDO_LOG (LANE_REDO_EXTERNAL_SIZE / ENTRY_SIZE) /* 40 */
/*
 * +10 chosen arbitrarily. This number of slots won't fit into the persistent
 * shadow log's initial capacity. It will cause it to grow once.
 */
#define SLOTS_NUM_MAX (SLOTS_NUM_MAX_BEFORE_GROWTH + 10) /* 70 */

struct root {
	uint64_t slots[SLOTS_NUM_MAX];
};

/*
 * It has to be big enough so the call counter won't reach this value naturally.
 */
#define BIG_ENOUGH_MAGIC_CALL_NUMBER 127

/*
 * The error injection is done for ulog_store().
 * The abort injection is done for ulog_process().
 *
 * Both of them are run one after another not only in case of processing
 * the user-built persistent shadow log but also whenever a reservation is
 * necessary e.g. when additional persistent redo log is needed to accomodate
 * the persistent shadow log. The persistent redo log reservation is done before
 * processing the persistent shadow log hence -1.
 *
 * ERROR_INJECT_CALL() and ABORTED_CALL() prime the respective call counter to
 * hit the dedicated magic value and trigger either an error injection or
 * an abort injection.
 */
#define _ERROR_INJECT_CALL BIG_ENOUGH_MAGIC_CALL_NUMBER
#define ERROR_INJECT_CALL(slots_num) \
	((slots_num > SLOTS_PER_REDO_LOG) ? \
		(_ERROR_INJECT_CALL - 1) : _ERROR_INJECT_CALL)

#define _ABORTED_CALL (BIG_ENOUGH_MAGIC_CALL_NUMBER * 2)
#define ABORTED_CALL(slots_num) \
	((slots_num > SLOTS_PER_REDO_LOG) ? \
		(_ABORTED_CALL - 1) : _ABORTED_CALL)

FUNC_MOCK(ulog_store, void, struct ulog *dest, struct ulog *src, size_t nbytes,
	size_t ulog_base_nbytes, size_t ulog_total_capacity,
	struct ulog_next *next, const struct pmem_ops *p_ops)
	/*
	 * Error injection was introduced to test if an error will be detected
	 * if the actual error happens. Only a subset of tests use error
	 * injection. The injected error is as envisioned by the issue that
	 * inspired this test's creation in the first place. In this case,
	 * the number of bytes truly populated in the persistent shadow log is
	 * replaced with the value reduced to the capacity of a single
	 * persistent redo log.
	 */
	FUNC_MOCK_RUN(_ERROR_INJECT_CALL) {
		_FUNC_REAL(ulog_store)(dest, src, LANE_REDO_EXTERNAL_SIZE,
			ulog_base_nbytes, ulog_total_capacity, next, p_ops);
		return;
	}
FUNC_MOCK_RUN_DEFAULT {
	_FUNC_REAL(ulog_store)(dest, src, nbytes, ulog_base_nbytes,
		ulog_total_capacity, next, p_ops);
}
FUNC_MOCK_END

FUNC_MOCK(ulog_process, void, struct ulog *ulog, ulog_check_offset_fn check,
	const struct pmem_ops *p_ops)
	/*
	 * The abort ought to be strategically injected just after copying
	 * the persistent shadow log to the persistent redo log but before
	 * processing it. So, when the pool is opened again the result of
	 * the sequence of the set-operations will rely solely on the contents
	 * of the persistent redo log not on the persistent shadow log.
	 */
	FUNC_MOCK_RUN(_ABORTED_CALL) {
		abort();
	}
FUNC_MOCK_RUN_DEFAULT {
	_FUNC_REAL(ulog_process)(ulog, check, p_ops);
}
FUNC_MOCK_END

#define ERROR_INJECTION_ON 1

static struct root *
get_root(PMEMobjpool *pop)
{
	PMEMoid root = pmemobj_root(pop, sizeof(struct root));
	if (OID_IS_NULL(root)) {
		UT_FATAL("!pmemobj_root: root == NULL");
	}
	struct root *rootp = (struct root *)pmemobj_direct(root);
	if (rootp == NULL) {
		UT_FATAL("pmemobj_direct: rootp == NULL");
	}
	return rootp;
}

/*
 * init -- create a PMEMOBJ pool and initialize the root object.
 */
static void
init(const char *path)
{
	PMEMobjpool *pop = pmemobj_create(path, LAYOUT_NAME, PMEMOBJ_MIN_POOL,
				S_IWUSR | S_IRUSR);
	if (pop == NULL) {
		UT_FATAL("!pmemobj_create: %s", path);
	}

	(void) get_root(pop);

	/* The root object is initially zeroed so no need to touch it. */

	pmemobj_close(pop);
}

/*
 * publish -- attempt to modify the values of the requested number of slots.
 * The redo log of the operation might be intentionally corrupted (an error
 * injection) and/or the process might be aborted just after writing
 * the redo log and before starting processing the published set-operations
 * (an abort injection).
 */
static void
publish(const char *path, int slots_num, bool error_inject, bool abort_inject)
{
	PMEMobjpool *pop = pmemobj_open(path, LAYOUT_NAME);
	if (pop == NULL) {
		UT_FATAL("!pmemobj_open: %s", path);
	}
	struct root *rootp = get_root(pop);

	struct pobj_action actions[SLOTS_NUM_MAX];
	unsigned actnum = 0;
	for (unsigned i = 0; i < slots_num; ++i) {
		pmemobj_set_value(pop, &actions[actnum++], &rootp->slots[i], 1);
	}

	/*
	 * prime the call counters if requested so an error injection or
	 * an abort injection will take place
	 */
	if (error_inject) {
		FUNC_MOCK_RCOUNTER_SET(ulog_store,
			ERROR_INJECT_CALL(slots_num));
	}
	if (abort_inject) {
		FUNC_MOCK_RCOUNTER_SET(ulog_process, ABORTED_CALL(slots_num));
	}
	/*
	 * The pmreorder markers help track down the operations belonging to
	 * the publish in question. Required for tests employing pmreorder.
	 */
	VALGRIND_PMC_EMIT_LOG("PMREORDER_PUBLISH.BEGIN");
	pmemobj_publish(pop, actions, actnum);
	VALGRIND_PMC_EMIT_LOG("PMREORDER_PUBLISH.END");

	pmemobj_close(pop);
}

/*
 * publish_abort_and_wait -- fork() the process and wait for the child to abort.
 * The child process will attempt to modify a requested number of slots' values
 * with or without error injection but it will abort just after writing
 * the redo log.
 */
static void
publish_abort_and_wait(const char *path, int slots_num, bool error_inject)
{
	int status;
	pid_t pid, ret;

	pid = fork();
	if (pid < 0) {
		UT_FATAL("!fork");
	}

	if (pid == 0) {
		const bool abort_inject = true;
		publish(path, slots_num, error_inject, abort_inject);
		UT_FATAL(
			"the child process should be aborted before this point");
	} else {
		ret = waitpid(pid, &status, 0);
		if (ret == -1) {
			UT_FATAL("!waitpid");
		}
		if (WIFEXITED(status)) {
			if (WEXITSTATUS(status) != SIGABRT_EXITSTATUS) {
				UT_FATAL(
					"the child terminated with an unexpected status: %d",
					WEXITSTATUS(status));
			}
		} else {
			UT_FATAL(
				"something unexpected happened to the child process");
		}
	}
}

/*
 * verify -- verify the requested number of slots are consistent. Either all
 * modified or all not modified.
 */
static void
verify(const char *path, int slots_num)
{
	PMEMobjpool *pop = pmemobj_open(path, LAYOUT_NAME);
	if (pop == NULL) {
		UT_FATAL("!pmemobj_open: %s", path);
	}

	struct root *rootp = get_root(pop);
	/*
	 * The correct state is when all the requested slots have exactly
	 * the same value.
	 */
	uint64_t exp = rootp->slots[0];

	for (unsigned i = 1; i < slots_num; ++i) {
		UT_ASSERTeq(rootp->slots[i], exp);
	}

	pmemobj_close(pop);
}

/* test entry points */

/*
 * test_init_publish_abort_and_verify -- execute the whole sequence with or
 * without error injection
 */
static int
test_init_publish_abort_and_verify(const struct test_case *tc, int argc,
	char *argv[])
{
	if (argc < 3) {
		UT_FATAL("usage: %s filename slots_num error_inject",
			__FUNCTION__);
	}

	const char *path = argv[0];
	int slots_num = atoi(argv[1]);
	bool error_inject = atoi(argv[2]) == ERROR_INJECTION_ON;

	init(path);
	publish_abort_and_wait(path, slots_num, error_inject);
	verify(path, slots_num);

	return 3;
}

/*
 * test_init -- just initialize the pool
 */
static int
test_init(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1) {
		UT_FATAL("usage: %s filename", __FUNCTION__);
	}

	const char *path = argv[0];
	init(path);

	return 1;
}

/*
 * test_publish -- having an initialized pool, change the values of
 * the requested number of slots, with or without error injection.
 */
static int
test_publish(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 3) {
		UT_FATAL("usage: %s filename slots_num error_inject",
			__FUNCTION__);
	}

	const char *path = argv[0];
	int slots_num = atoi(argv[1]);
	bool error_inject = atoi(argv[2]) == ERROR_INJECTION_ON;

	const bool abort_inject = false;
	publish(path, slots_num, error_inject, abort_inject);

	return 3;
}

/*
 * test_verify -- verify the state of the requested number of slots.
 */
static int
test_verify(const struct test_case *tc, int argc, char *argv[])
{
	/*
	 * Note: the file name has to be the last argument. It is forced by
	 * pmreorder.
	 */
	if (argc < 2) {
		UT_FATAL("usage: %s slots_num filename", __FUNCTION__);
	}

	int slots_num = atoi(argv[0]);
	const char *path = argv[1];

	/*
	 * The setting preferred by the pmreorder's verify implementations.
	 */
	int y = 1;
	pmemobj_ctl_set(NULL, "copy_on_write.at_open", &y);

	verify(path, slots_num);

	/*
	 * If the verify did not fail till now it has passed successfully.
	 * Return the result ASAP.
	 */
	END(0);
}

static struct test_case test_cases[] = {
	TEST_CASE(test_init_publish_abort_and_verify),
	TEST_CASE(test_init),
	TEST_CASE(test_publish),
	TEST_CASE(test_verify),
};

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_ulog_advanced");

	/*
	 * Assert the assumptions.
	 * Please see the description at the beginning of this file.
	 */
	COMPILE_ERROR_ON(ULOG_BASE_SIZE != 1024);
	COMPILE_ERROR_ON(CACHELINE_SIZE != 64);
	COMPILE_ERROR_ON(LANE_REDO_EXTERNAL_SIZE != 640);
	COMPILE_ERROR_ON(sizeof(struct ulog_entry_val) != 16);

	TEST_CASE_PROCESS(argc, argv, test_cases, ARRAY_SIZE(test_cases));

	DONE(NULL);
}
