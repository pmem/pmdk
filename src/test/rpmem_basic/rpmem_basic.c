/*
 * Copyright 2016-2018, Intel Corporation
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
 * rpmem_basic.c -- unit test for rpmem operations
 */

#include "unittest.h"

#include "librpmem.h"
#include "pool_hdr.h"
#include "set.h"
#include "util.h"
#include "out.h"

#include "rpmem_common.h"
#include "rpmem_fip_common.h"

/*
 * Use default terminal command for terminating session in user flags field
 * in order to make sure this is not interpreted by terminal.
 */
#define POOL_ATTR_INIT {\
	.signature		= "<RPMEM>",\
	.major			= 1,\
	.compat_features	= 2,\
	.incompat_features	= 3,\
	.ro_compat_features	= 4,\
	.poolset_uuid		= "POOLSET_UUID0123",\
	.uuid			= "UUID0123456789AB",\
	.next_uuid		= "NEXT_UUID0123456",\
	.prev_uuid		= "PREV_UUID0123456",\
	.user_flags		= "USER_FLAGS\0\0\0\n~.",\
}

#define POOL_ATTR_ALT {\
	.signature		= "<ALT>",\
	.major			= 5,\
	.compat_features	= 6,\
	.incompat_features	= 7,\
	.ro_compat_features	= 8,\
	.poolset_uuid		= "UUID_POOLSET_ALT",\
	.uuid			= "ALT_UUIDCDEFFEDC",\
	.next_uuid		= "456UUID_NEXT_ALT",\
	.prev_uuid		= "UUID012_ALT_PREV",\
	.user_flags		= "\0\0\0\n~._ALT_FLAGS",\
}

const struct rpmem_pool_attr pool_attrs[] = {
	POOL_ATTR_INIT,
	POOL_ATTR_ALT
};

const char *pool_attr_names[] = {
	"init",
	"alt"
};

#define POOL_ATTR_INIT_INDEX	0

#define NLANES	32

struct pool_entry {
	RPMEMpool *rpp;
	const char *target;
	void *pool;
	size_t size;
	int is_mem;
	int error_must_occur;
	int exp_errno;
};

#define MAX_IDS	1024
struct pool_entry pools[MAX_IDS];

/*
 * init_pool -- map local pool file or allocate memory region
 */
static void
init_pool(struct pool_entry *pool, const char *target, const char *pool_path,
	const char *pool_size)
{
	pool->target = target;
	pool->exp_errno = 0;

	int ret = util_parse_size(pool_size, &pool->size);
	UT_ASSERTeq(ret, 0);

	int flags = PMEM_FILE_CREATE;
	if (pool->size)
		flags |= PMEM_FILE_EXCL;


	if (strcmp(pool_path, "mem") == 0) {
		pool->pool = PAGEALIGNMALLOC(pool->size);

		pool->is_mem = 1;
	} else {
		pool->pool = pmem_map_file(pool_path, pool->size,
			flags, 0666, &pool->size, NULL);
		UT_ASSERTne(pool->pool, NULL);

		/*
		 * This is a workaround for an issue with using device dax with
		 * libibverbs. The problem is that libfabric to handle fork()
		 * function calls correctly use ibv_fork_init(3) which makes
		 * all registered memory being madvised with MADV_DONTFORK flag.
		 * In libpmemobj the remote replication is performed without
		 * pool header (first 4k). In such case the address passed to
		 * madvise(2) is aligned to 4k, but device dax can require
		 * different alignment (default is 2MB). This workaround
		 * madvises the entire memory region before registering
		 * it by fi_mr_reg(3).
		 *
		 * The librpmem client requires fork() support to work
		 * correctly.
		 */
		ret = os_madvise(pool->pool, pool->size, MADV_DONTFORK);

		UT_ASSERTeq(ret, 0);

		pool->is_mem = 0;
		os_unlink(pool_path);
	}
}

/*
 * free_pool -- unmap local pool file or free memory region
 */
static void
free_pool(struct pool_entry *pool)
{
	if (pool->is_mem)
		FREE(pool->pool);
	else
		UT_ASSERTeq(pmem_unmap(pool->pool, pool->size), 0);

	pool->pool = NULL;
	pool->rpp = NULL;
	pool->target = NULL;
}

/*
 * str_2_pool_attr_index -- convert string to the index of pool attributes
 */
static int
str_2_pool_attr_index(const char *str)
{
	COMPILE_ERROR_ON((sizeof(pool_attr_names) / sizeof(pool_attr_names[0]))
		!= (sizeof(pool_attrs) / sizeof(pool_attrs[0])));

	const unsigned num_of_names = sizeof(pool_attr_names) /
		sizeof(pool_attr_names[0]);
	for (int i = 0; i < num_of_names; ++i) {
		if (strcmp(str, pool_attr_names[i]) == 0) {
			return i;
		}
	}

	UT_FATAL("unrecognized name of pool attributes set: %s", str);
}

/*
 * cmp_pool_attr -- check pool attributes
 */
static void
cmp_pool_attr(const struct rpmem_pool_attr *attr1,
	const struct rpmem_pool_attr *attr2)
{
	UT_ASSERTeq(memcmp(attr1->signature, attr2->signature,
				sizeof(attr1->signature)), 0);
	UT_ASSERTeq(attr1->major, attr2->major);
	UT_ASSERTeq(attr1->compat_features, attr2->compat_features);
	UT_ASSERTeq(attr1->ro_compat_features, attr2->ro_compat_features);
	UT_ASSERTeq(attr1->incompat_features, attr2->incompat_features);
	UT_ASSERTeq(memcmp(attr1->uuid, attr2->uuid,
				sizeof(attr1->uuid)), 0);
	UT_ASSERTeq(memcmp(attr1->poolset_uuid, attr2->poolset_uuid,
				sizeof(attr1->poolset_uuid)), 0);
	UT_ASSERTeq(memcmp(attr1->prev_uuid, attr2->prev_uuid,
				sizeof(attr1->prev_uuid)), 0);
	UT_ASSERTeq(memcmp(attr1->next_uuid, attr2->next_uuid,
				sizeof(attr1->next_uuid)), 0);
}

/*
 * check_return_and_errno - validate return value and errno
 */
#define check_return_and_errno(ret, error_must_occur, exp_errno) \
	if (exp_errno != 0) { \
		if (error_must_occur) { \
			UT_ASSERTne(ret, 0); \
			UT_ASSERTeq(errno, exp_errno); \
		} else { \
			if (ret != 0) { \
				UT_ASSERTeq(errno, exp_errno); \
			} \
		} \
	} else { \
		UT_ASSERTeq(ret, 0); \
	}

/*
 * test_create -- test case for creating remote pool
 */
static int
test_create(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 5)
		UT_FATAL("usage: test_create <id> <pool set> "
				"<target> <pool> <size>");

	const char *id_str = argv[0];
	const char *pool_set = argv[1];
	const char *target = argv[2];
	const char *pool_path = argv[3];
	const char *size_str = argv[4];

	unsigned nlanes = NLANES;
	int id = atoi(id_str);
	UT_ASSERT(id >= 0 && id < MAX_IDS);
	struct pool_entry *pool = &pools[id];
	UT_ASSERTeq(pool->rpp, NULL);

	init_pool(pool, target, pool_path, size_str);

	struct rpmem_pool_attr pool_attr = pool_attrs[POOL_ATTR_INIT_INDEX];
	pool->rpp = rpmem_create(target, pool_set, pool->pool,
			pool->size, &nlanes, &pool_attr);

	if (pool->rpp) {
		UT_ASSERTne(nlanes, 0);
		UT_OUT("%s: created", pool_set);
	} else {
		UT_OUT("!%s", pool_set);
		free_pool(pool);
	}

	return 5;
}

/*
 * test_open -- test case for opening remote pool
 */
static int
test_open(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 6)
		UT_FATAL("usage: test_open <id> <pool set> "
				"<target> <pool> <pool attr name>");

	const char *id_str = argv[0];
	const char *pool_set = argv[1];
	const char *target = argv[2];
	const char *pool_path = argv[3];
	const char *size_str = argv[4];
	const char *pool_attr_name = argv[5];

	int id = atoi(id_str);
	UT_ASSERT(id >= 0 && id < MAX_IDS);
	struct pool_entry *pool = &pools[id];
	UT_ASSERTeq(pool->rpp, NULL);
	const int pool_attr_id = str_2_pool_attr_index(pool_attr_name);

	unsigned nlanes = NLANES;

	init_pool(pool, target, pool_path, size_str);

	struct rpmem_pool_attr pool_attr;
	pool->rpp = rpmem_open(target, pool_set, pool->pool,
			pool->size, &nlanes, &pool_attr);

	if (pool->rpp) {
		cmp_pool_attr(&pool_attr, &pool_attrs[pool_attr_id]);
		UT_ASSERTne(nlanes, 0);

		UT_OUT("%s: opened", pool_set);
	} else {
		UT_OUT("!%s", pool_set);
		free_pool(pool);
	}

	return 6;
}

/*
 * test_close -- test case for closing remote pool
 */
static int
test_close(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_close <id>");

	const char *id_str = argv[0];
	int id = atoi(id_str);
	UT_ASSERT(id >= 0 && id < MAX_IDS);
	struct pool_entry *pool = &pools[id];
	UT_ASSERTne(pool->rpp, NULL);

	int ret = rpmem_close(pool->rpp);
	check_return_and_errno(ret, pool->error_must_occur, pool->exp_errno);

	free_pool(pool);

	return 1;
}

/*
 * thread_arg -- persist worker thread arguments
 */
struct thread_arg {
	RPMEMpool *rpp;
	size_t off;
	size_t size;
	int nops;
	unsigned lane;
	int error_must_occur;
	int exp_errno;
};

/*
 * persist_thread -- persist worker thread function
 */
static void *
persist_thread(void *arg)
{
	struct thread_arg *args = arg;
	size_t persist_size = args->size / args->nops;
	UT_ASSERTeq(args->size % args->nops, 0);
	for (int i = 0; i < args->nops; i++) {
		size_t off = args->off + i * persist_size;
		size_t left = args->size - i * persist_size;
		size_t size = left < persist_size ?
				left : persist_size;

		int ret = rpmem_persist(args->rpp, off, size, args->lane);
		check_return_and_errno(ret, args->error_must_occur,
				args->exp_errno);
	}

	return NULL;
}

/*
 * test_persist -- test case for persist operation
 */
static int
test_persist(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 4)
		UT_FATAL("usage: test_persist <id> <seed> <nthreads> <nops>");

	int id = atoi(argv[0]);
	UT_ASSERT(id >= 0 && id < MAX_IDS);
	struct pool_entry *pool = &pools[id];
	int seed = atoi(argv[1]);

	int nthreads = atoi(argv[2]);
	int nops = atoi(argv[3]);

	size_t buff_size = pool->size - POOL_HDR_SIZE;

	if (seed) {
		srand(seed);
		uint8_t *buff = (uint8_t *)((uintptr_t)pool->pool +
				POOL_HDR_SIZE);
		for (size_t i = 0; i < buff_size; i++)
			buff[i] = rand();
	}

	os_thread_t *threads = MALLOC(nthreads * sizeof(*threads));
	struct thread_arg *args = MALLOC(nthreads * sizeof(*args));
	size_t size_per_thread = buff_size / nthreads;
	UT_ASSERTeq(buff_size % nthreads, 0);

	for (int i = 0; i < nthreads; i++) {
		args[i].rpp = pool->rpp;
		args[i].nops = nops;
		args[i].lane = (unsigned)i;
		args[i].off = POOL_HDR_SIZE + i * size_per_thread;
		size_t size_left = buff_size - size_per_thread * i;
		args[i].size = size_left < size_per_thread ?
				size_left : size_per_thread;
		args[i].exp_errno = pool->exp_errno;
		args[i].error_must_occur = pool->error_must_occur;
		PTHREAD_CREATE(&threads[i], NULL, persist_thread, &args[i]);
	}

	for (int i = 0; i < nthreads; i++)
		PTHREAD_JOIN(&threads[i], NULL);

	FREE(args);
	FREE(threads);

	return 4;
}

/*
 * test_read -- test case for read operation
 */
static int
test_read(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_read <id> <seed>");

	int id = atoi(argv[0]);
	UT_ASSERT(id >= 0 && id < MAX_IDS);
	struct pool_entry *pool = &pools[id];

	srand(atoi(argv[1]));

	int ret;
	uint8_t *buff = (uint8_t *)((uintptr_t)pool->pool + POOL_HDR_SIZE);
	size_t buff_size = pool->size - POOL_HDR_SIZE;

	ret = rpmem_read(pool->rpp, buff, POOL_HDR_SIZE, buff_size, 0);
	check_return_and_errno(ret, pool->error_must_occur, pool->exp_errno);

	if (ret == 0) {
		for (size_t i = 0; i < buff_size; i++) {
			uint8_t r = rand();
			UT_ASSERTeq(buff[i], r);
		}
	}

	return 2;
}

/*
 * test_remove -- test case for remove operation
 */
static int
test_remove(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 4)
		UT_FATAL("usage: test_remove <target> <pool set> "
			"<force> <rm pool set>");

	const char *target = argv[0];
	const char *pool_set = argv[1];
	int force = atoi(argv[2]);
	int rm_pool_set = atoi(argv[3]);

	int flags = 0;

	if (force)
		flags |= RPMEM_REMOVE_FORCE;

	if (rm_pool_set)
		flags |= RPMEM_REMOVE_POOL_SET;

	int ret;

	ret = rpmem_remove(target, pool_set, flags);
	UT_ASSERTeq(ret, 0);

	return 4;
}

/*
 * test_set_attr -- test case for set attributes operation
 */
static int
test_set_attr(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_set_attr <id> <pool attr name>");

	const char *id_str = argv[0];
	const char *pool_attr_name = argv[1];

	int id = atoi(id_str);
	UT_ASSERT(id >= 0 && id < MAX_IDS);
	struct pool_entry *pool = &pools[id];
	UT_ASSERTne(pool->rpp, NULL);
	const int pool_attr_id = str_2_pool_attr_index(pool_attr_name);

	int ret = rpmem_set_attr(pool->rpp, &pool_attrs[pool_attr_id]);
	UT_ASSERTeq(ret, 0);

	UT_OUT("set attributes succeeded (%s)", pool_attr_name);

	return 2;
}

/*
 * check_pool -- check if remote pool contains specified random sequence
 */
static int
check_pool(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 3)
		UT_FATAL("usage: fill_pool <pool set> <seed> <size>");

	char *pool_set = argv[0];
	srand(atoi(argv[1]));

	int ret;

	size_t size;
	ret = util_parse_size(argv[2], &size);
	size -= POOL_HDR_SIZE;

	struct pool_set *set;
	ret = util_poolset_create_set(&set, pool_set, 0, 0, 0);
	UT_ASSERTeq(ret, 0);
	ret = util_pool_open_nocheck(set, 0);
	UT_ASSERTeq(ret, 0);

	uint8_t *data = set->replica[0]->part[0].addr;
	for (size_t i = 0; i < size; i++) {
		uint8_t r = rand();
		UT_ASSERTeq(data[POOL_HDR_SIZE + i], r);
	}

	util_poolset_close(set, DO_NOT_DELETE_PARTS);

	return 3;
}

/*
 * fill_pool -- fill remote pool with specified random sequence
 */
static int
fill_pool(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: fill_pool <pool set> <seed>");

	char *pool_set = argv[0];
	srand(atoi(argv[1]));

	int ret;

	struct pool_set *set;
	ret = util_poolset_create_set(&set, pool_set, 0, 0, 0);
	UT_ASSERTeq(ret, 0);
	ret = util_pool_open_nocheck(set, 0);
	UT_ASSERTeq(ret, 0);

	uint8_t *data = set->replica[0]->part[0].addr;
	for (size_t i = POOL_HDR_SIZE; i < set->poolsize; i++)
		data[i] = rand();

	util_poolset_close(set, DO_NOT_DELETE_PARTS);

	return 2;
}

enum wait_type {
	WAIT,
	NOWAIT
};

const char *wait_type_str[2] = {
	"wait",
	"nowait"
};

static enum wait_type
str2wait(const char *str)
{
	for (int i = 0; i < ARRAY_SIZE(wait_type_str); ++i) {
		if (strcmp(wait_type_str[i], str) == 0)
			return (enum wait_type)i;
	}

	UT_FATAL("'%s' does not match <wait|nowait>", str);
}

#define SSH_EXE "ssh"
#define RPMEMD_TERMINATE_CMD SSH_EXE " -tt %s kill -9 %d"
#define GET_RPMEMD_PID_CMD SSH_EXE " %s cat %s"
#define COUNT_RPMEMD_CMD SSH_EXE " %s ps -A | grep -c %d"

/*
 * rpmemd_kill -- kill target rpmemd
 */
static int
rpmemd_kill(const char *target, int pid)
{
	char cmd[100];
	snprintf(cmd, sizeof(cmd), RPMEMD_TERMINATE_CMD, target, pid);
	return system(cmd);
}

/*
 * popen_readi -- popen cmd and read integer
 */
static int
popen_readi(const char *cmd)
{
	FILE *stream = popen(cmd, "r");
	UT_ASSERT(stream != NULL);

	int i;
	int ret = fscanf(stream, "%d", &i);
	UT_ASSERT(ret == 1);

	pclose(stream);

	return i;
}

/*
 * rpmemd_get_pid -- get target rpmemd pid
 */
static int
rpmemd_get_pid(const char *target, const char *pid_file)
{
	char cmd[PATH_MAX];
	snprintf(cmd, sizeof(cmd), GET_RPMEMD_PID_CMD, target, pid_file);
	return popen_readi(cmd);
}

/*
 * rpmemd_is_running -- tell if target rpmemd is running
 */
static int
rpmemd_is_running(const char *target, int pid)
{
	char cmd[100];
	snprintf(cmd, sizeof(cmd), COUNT_RPMEMD_CMD, target, pid);
	return popen_readi(cmd) > 0;
}

/*
 * rpmemd_kill_wait -- kill target rpmemd and wait for it to stop
 */
static void
rpmemd_kill_wait(const char *target, const char *pid_file, enum wait_type wait)
{
	int pid = rpmemd_get_pid(target, pid_file);
	int ret;

	do {
		ret = rpmemd_kill(target, pid);
		if (ret != 0 || wait == NOWAIT) {
			break;
		}
	} while (rpmemd_is_running(target, pid));
}

/*
 * rpmemd_terminate -- terminate target rpmemd
 */
static int
rpmemd_terminate(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 3) {
		UT_FATAL("usage: rpmemd_terminate <id> <pid file> "
				"<wait|nowait>");
	}

	const char *id_str = argv[0];
	const char *pid_file = argv[1];
	const char *wait_str = argv[2];

	int id = atoi(id_str);
	UT_ASSERT(id >= 0 && id < MAX_IDS);

	struct pool_entry *pool = &pools[id];
	UT_ASSERTne(pool->target, NULL);
	pool->exp_errno = ECONNRESET;

	enum wait_type wait = str2wait(wait_str);
	/*
	 * if process will wait for rpmemd to terminate it is sure error will
	 * occur
	 */
	pool->error_must_occur = wait == WAIT;
	rpmemd_kill_wait(pool->target, pid_file, wait);

	return 3;
}

/*
 * test_persist_illegal -- test case for persisting data with offset < 4096
 */
static int
test_persist_illegal(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_persist_illegal <id>");

	int id = atoi(argv[0]);
	UT_ASSERT(id >= 0 && id < MAX_IDS);
	struct pool_entry *pool = &pools[id];

	for (size_t off = 0; off < POOL_HDR_SIZE; ++off) {
		int ret = rpmem_persist(pool->rpp, off, 1, 0);
		UT_ASSERTeq(ret, -1);
	}

	return 1;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_create),
	TEST_CASE(test_open),
	TEST_CASE(test_set_attr),
	TEST_CASE(test_close),
	TEST_CASE(test_persist),
	TEST_CASE(test_read),
	TEST_CASE(test_remove),
	TEST_CASE(check_pool),
	TEST_CASE(fill_pool),
	TEST_CASE(rpmemd_terminate),
	TEST_CASE(test_persist_illegal),
};

#define NTESTS	(sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char *argv[])
{
	util_init();
	rpmem_fip_probe_get("localhost", NULL);
	START(argc, argv, "rpmem_basic");

	out_init("rpmem_basic", "TEST_LOG_LEVEL", "TEST_LOG_FILE", 0, 0);

	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);

	out_fini();

	DONE(NULL);
}
