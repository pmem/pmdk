// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2018, Intel Corporation */

/*
 * obj_locks.cpp -- main source file for PMEM locks benchmark
 */

#include <cassert>
#include <cerrno>

#include "benchmark.hpp"
#include "libpmemobj.h"

#include "file.h"
#include "lane.h"
#include "list.h"
#include "memops.h"
#include "obj.h"
#include "os_thread.h"
#include "out.h"
#include "pmalloc.h"
#include "sync.h"

struct prog_args {
	bool use_system_threads; /* use system locks instead of PMEM locks */
	unsigned n_locks;	/* number of mutex/rwlock objects */
	bool run_id_increment;   /* increment run_id after each lock/unlock */
	uint64_t runid_initial_value; /* initial value of run_id */
	char *lock_mode;	      /* "1by1" or "all-lock" */
	char *lock_type;	      /* "mutex", "rwlock" or "ram-mutex" */
	bool use_rdlock;	      /* use read lock, instead of write lock */
};

/*
 * mutex similar to PMEMmutex, but with os_mutex_t in RAM
 */
typedef union padded_volatile_pmemmutex {
	char padding[_POBJ_CL_SIZE];
	struct {
		uint64_t runid;
		os_mutex_t *mutexp; /* pointer to os_thread mutex in RAM */
	} volatile_pmemmutex;
} PMEM_volatile_mutex;

typedef union lock_union {
	PMEMmutex pm_mutex;
	PMEMrwlock pm_rwlock;
	PMEM_volatile_mutex pm_vmutex;
	os_mutex_t pt_mutex;
	os_rwlock_t pt_rwlock;
} lock_t;

POBJ_LAYOUT_BEGIN(pmembench_lock_layout);
POBJ_LAYOUT_ROOT(pmembench_lock_layout, struct my_root);
POBJ_LAYOUT_TOID(pmembench_lock_layout, lock_t);
POBJ_LAYOUT_END(pmembench_lock_layout);

/*
 * my_root -- root object structure
 */
struct my_root {
	TOID(lock_t) locks; /* an array of locks */
};

/*
 * lock usage
 */
enum operation_mode {
	OP_MODE_1BY1,     /* lock and unlock one lock at a time */
	OP_MODE_ALL_LOCK, /* grab all locks, then unlock them all */
	OP_MODE_MAX,
};

/*
 * lock type
 */
enum benchmark_mode {
	BENCH_MODE_MUTEX,	  /* PMEMmutex vs. os_mutex_t */
	BENCH_MODE_RWLOCK,	 /* PMEMrwlock vs. os_rwlock_t */
	BENCH_MODE_VOLATILE_MUTEX, /* PMEMmutex with os_thread mutex in RAM */
	BENCH_MODE_MAX
};

struct mutex_bench;

struct bench_ops {
	int (*bench_init)(struct mutex_bench *);
	int (*bench_exit)(struct mutex_bench *);
	int (*bench_op)(struct mutex_bench *);
};

/*
 * mutex_bench -- stores variables used in benchmark, passed within functions
 */
struct mutex_bench {
	PMEMobjpool *pop;	      /* pointer to the persistent pool */
	TOID(struct my_root) root;     /* OID of the root object */
	struct prog_args *pa;	  /* prog_args structure */
	enum operation_mode lock_mode; /* lock usage mode */
	enum benchmark_mode lock_type; /* lock type */
	lock_t *locks;		       /* pointer to the array of locks */
	struct bench_ops *ops;
};

#define GET_VOLATILE_MUTEX(pop, mutexp)                                        \
	(os_mutex_t *)get_lock(                                                \
		(pop)->run_id, &(mutexp)->volatile_pmemmutex.runid,            \
		(mutexp)->volatile_pmemmutex.mutexp,                           \
		(int (*)(void **lock, void *arg))volatile_mutex_init)

typedef int (*lock_fun_wrapper)(PMEMobjpool *pop, void *lock);

/*
 * bench_operation_1by1 -- acquire lock and unlock release locks
 */
static void
bench_operation_1by1(lock_fun_wrapper flock, lock_fun_wrapper funlock,
		     struct mutex_bench *mb, PMEMobjpool *pop)
{
	for (unsigned i = 0; i < (mb)->pa->n_locks; (i)++) {
		auto *o = (void *)(&(mb)->locks[i]);
		flock(pop, o);
		funlock(pop, o);
	}
}

/*
 * bench_operation_all_lock -- acquire all locks and release all locks
 */
static void
bench_operation_all_lock(lock_fun_wrapper flock, lock_fun_wrapper funlock,
			 struct mutex_bench *mb, PMEMobjpool *pop)
{
	for (unsigned i = 0; i < (mb)->pa->n_locks; (i)++) {
		auto *o = (void *)(&(mb)->locks[i]);
		flock(pop, o);
	}
	for (unsigned i = 0; i < (mb)->pa->n_locks; i++) {
		auto *o = (void *)(&(mb)->locks[i]);
		funlock(pop, o);
	}
}

/*
 * get_lock -- atomically initialize and return a lock
 */
static void *
get_lock(uint64_t pop_runid, volatile uint64_t *runid, void *lock,
	 int (*init_lock)(void **lock, void *arg))
{
	uint64_t tmp_runid;
	while ((tmp_runid = *runid) != pop_runid) {
		if ((tmp_runid != (pop_runid - 1))) {
			if (util_bool_compare_and_swap64(runid, tmp_runid,
							 (pop_runid - 1))) {
				if (init_lock(&lock, nullptr)) {
					util_fetch_and_and64(runid, 0);
					return nullptr;
				}

				if (util_bool_compare_and_swap64(
					    runid, (pop_runid - 1),
					    pop_runid) == 0) {
					return nullptr;
				}
			}
		}
	}
	return lock;
}

/*
 * volatile_mutex_init -- initialize the volatile mutex object
 *
 * Allocate memory for the os_thread mutex and initialize it.
 * Set the runid to the same value as in the memory pool.
 */
static int
volatile_mutex_init(os_mutex_t **mutexp, void *attr)
{
	if (*mutexp == nullptr) {
		*mutexp = (os_mutex_t *)malloc(sizeof(os_mutex_t));
		if (*mutexp == nullptr) {
			perror("volatile_mutex_init alloc");
			return ENOMEM;
		}
	}

	return os_mutex_init(*mutexp);
}

/*
 * volatile_mutex_lock -- initialize the mutex object if needed and lock it
 */
static int
volatile_mutex_lock(PMEMobjpool *pop, PMEM_volatile_mutex *mutexp)
{
	auto *mutex = GET_VOLATILE_MUTEX(pop, mutexp);
	if (mutex == nullptr)
		return EINVAL;

	return os_mutex_lock(mutex);
}

/*
 * volatile_mutex_unlock -- unlock the mutex
 */
static int
volatile_mutex_unlock(PMEMobjpool *pop, PMEM_volatile_mutex *mutexp)
{
	auto *mutex = (os_mutex_t *)GET_VOLATILE_MUTEX(pop, mutexp);
	if (mutex == nullptr)
		return EINVAL;

	return os_mutex_unlock(mutex);
}

/*
 * volatile_mutex_destroy -- destroy os_thread mutex and release memory
 */
static int
volatile_mutex_destroy(PMEMobjpool *pop, PMEM_volatile_mutex *mutexp)
{
	auto *mutex = (os_mutex_t *)GET_VOLATILE_MUTEX(pop, mutexp);
	if (mutex == nullptr)
		return EINVAL;

	int ret = os_mutex_destroy(mutex);
	if (ret != 0)
		return ret;

	free(mutex);

	return 0;
}

/*
 * os_mutex_lock_wrapper -- wrapper for os_mutex_lock
 */
static int
os_mutex_lock_wrapper(PMEMobjpool *pop, void *lock)
{
	return os_mutex_lock((os_mutex_t *)lock);
}

/*
 * os_mutex_unlock_wrapper -- wrapper for os_mutex_unlock
 */
static int
os_mutex_unlock_wrapper(PMEMobjpool *pop, void *lock)
{
	return os_mutex_unlock((os_mutex_t *)lock);
}

/*
 * pmemobj_mutex_lock_wrapper -- wrapper for pmemobj_mutex_lock
 */
static int
pmemobj_mutex_lock_wrapper(PMEMobjpool *pop, void *lock)
{
	return pmemobj_mutex_lock(pop, (PMEMmutex *)lock);
}

/*
 * pmemobj_mutex_unlock_wrapper -- wrapper for pmemobj_mutex_unlock
 */
static int
pmemobj_mutex_unlock_wrapper(PMEMobjpool *pop, void *lock)
{
	return pmemobj_mutex_unlock(pop, (PMEMmutex *)lock);
}

/*
 * os_rwlock_wrlock_wrapper -- wrapper for os_rwlock_wrlock
 */
static int
os_rwlock_wrlock_wrapper(PMEMobjpool *pop, void *lock)
{
	return os_rwlock_wrlock((os_rwlock_t *)lock);
}

/*
 * os_rwlock_rdlock_wrapper -- wrapper for os_rwlock_rdlock
 */
static int
os_rwlock_rdlock_wrapper(PMEMobjpool *pop, void *lock)
{
	return os_rwlock_rdlock((os_rwlock_t *)lock);
}

/*
 * os_rwlock_unlock_wrapper -- wrapper for os_rwlock_unlock
 */
static int
os_rwlock_unlock_wrapper(PMEMobjpool *pop, void *lock)
{
	return os_rwlock_unlock((os_rwlock_t *)lock);
}

/*
 * pmemobj_rwlock_wrlock_wrapper -- wrapper for pmemobj_rwlock_wrlock
 */
static int
pmemobj_rwlock_wrlock_wrapper(PMEMobjpool *pop, void *lock)
{
	return pmemobj_rwlock_wrlock(pop, (PMEMrwlock *)lock);
}

/*
 * pmemobj_rwlock_rdlock_wrapper -- wrapper for pmemobj_rwlock_rdlock
 */
static int
pmemobj_rwlock_rdlock_wrapper(PMEMobjpool *pop, void *lock)
{
	return pmemobj_rwlock_rdlock(pop, (PMEMrwlock *)lock);
}

/*
 * pmemobj_rwlock_unlock_wrapper -- wrapper for pmemobj_rwlock_unlock
 */
static int
pmemobj_rwlock_unlock_wrapper(PMEMobjpool *pop, void *lock)
{
	return pmemobj_rwlock_unlock(pop, (PMEMrwlock *)lock);
}

/*
 * volatile_mutex_lock_wrapper -- wrapper for volatile_mutex_lock
 */
static int
volatile_mutex_lock_wrapper(PMEMobjpool *pop, void *lock)
{
	return volatile_mutex_lock(pop, (PMEM_volatile_mutex *)lock);
}

/*
 * volatile_mutex_unlock_wrapper -- wrapper for volatile_mutex_unlock
 */
static int
volatile_mutex_unlock_wrapper(PMEMobjpool *pop, void *lock)
{
	return volatile_mutex_unlock(pop, (PMEM_volatile_mutex *)lock);
}

/*
 * init_bench_mutex -- allocate and initialize mutex objects
 */
static int
init_bench_mutex(struct mutex_bench *mb)
{
	POBJ_ZALLOC(mb->pop, &D_RW(mb->root)->locks, lock_t,
		    mb->pa->n_locks * sizeof(lock_t));
	if (TOID_IS_NULL(D_RO(mb->root)->locks)) {
		perror("POBJ_ZALLOC");
		return -1;
	}

	struct my_root *root = D_RW(mb->root);
	assert(root != nullptr);
	mb->locks = D_RW(root->locks);
	assert(mb->locks != nullptr);

	if (!mb->pa->use_system_threads) {
		/* initialize PMEM mutexes */
		for (unsigned i = 0; i < mb->pa->n_locks; i++) {
			auto *p = (PMEMmutex_internal *)&mb->locks[i];
			p->pmemmutex.runid = mb->pa->runid_initial_value;
			os_mutex_init(&p->PMEMmutex_lock);
		}
	} else {
		/* initialize os_thread mutexes */
		for (unsigned i = 0; i < mb->pa->n_locks; i++) {
			auto *p = (os_mutex_t *)&mb->locks[i];
			os_mutex_init(p);
		}
	}

	return 0;
}

/*
 * exit_bench_mutex -- destroy the mutex objects and release memory
 */
static int
exit_bench_mutex(struct mutex_bench *mb)
{
	if (mb->pa->use_system_threads) {
		/* deinitialize os_thread mutex objects */
		for (unsigned i = 0; i < mb->pa->n_locks; i++) {
			auto *p = (os_mutex_t *)&mb->locks[i];
			os_mutex_destroy(p);
		}
	}

	POBJ_FREE(&D_RW(mb->root)->locks);

	return 0;
}

/*
 * op_bench_mutex -- lock and unlock the mutex object
 *
 * If requested, increment the run_id of the memory pool.  In case of PMEMmutex
 * this will force the rwlock object(s) reinitialization at the lock operation.
 */
static int
op_bench_mutex(struct mutex_bench *mb)
{
	if (!mb->pa->use_system_threads) {
		if (mb->lock_mode == OP_MODE_1BY1) {
			bench_operation_1by1(pmemobj_mutex_lock_wrapper,
					     pmemobj_mutex_unlock_wrapper, mb,
					     mb->pop);
		} else {
			bench_operation_all_lock(pmemobj_mutex_lock_wrapper,
						 pmemobj_mutex_unlock_wrapper,
						 mb, mb->pop);
		}
		if (mb->pa->run_id_increment)
			mb->pop->run_id += 2; /* must be a multiple of 2 */
	} else {
		if (mb->lock_mode == OP_MODE_1BY1) {
			bench_operation_1by1(os_mutex_lock_wrapper,
					     os_mutex_unlock_wrapper, mb,
					     nullptr);
		} else {
			bench_operation_all_lock(os_mutex_lock_wrapper,
						 os_mutex_unlock_wrapper, mb,
						 nullptr);
		}
	}

	return 0;
}

/*
 * init_bench_rwlock -- allocate and initialize rwlock objects
 */
static int
init_bench_rwlock(struct mutex_bench *mb)
{
	struct my_root *root = D_RW(mb->root);
	assert(root != nullptr);

	POBJ_ZALLOC(mb->pop, &root->locks, lock_t,
		    mb->pa->n_locks * sizeof(lock_t));
	if (TOID_IS_NULL(root->locks)) {
		perror("POBJ_ZALLOC");
		return -1;
	}

	mb->locks = D_RW(root->locks);
	assert(mb->locks != nullptr);

	if (!mb->pa->use_system_threads) {
		/* initialize PMEM rwlocks */
		for (unsigned i = 0; i < mb->pa->n_locks; i++) {
			auto *p = (PMEMrwlock_internal *)&mb->locks[i];
			p->pmemrwlock.runid = mb->pa->runid_initial_value;
			os_rwlock_init(&p->PMEMrwlock_lock);
		}
	} else {
		/* initialize os_thread rwlocks */
		for (unsigned i = 0; i < mb->pa->n_locks; i++) {
			auto *p = (os_rwlock_t *)&mb->locks[i];
			os_rwlock_init(p);
		}
	}

	return 0;
}

/*
 * exit_bench_rwlock -- destroy the rwlocks and release memory
 */
static int
exit_bench_rwlock(struct mutex_bench *mb)
{
	if (mb->pa->use_system_threads) {
		/* deinitialize os_thread mutex objects */
		for (unsigned i = 0; i < mb->pa->n_locks; i++) {
			auto *p = (os_rwlock_t *)&mb->locks[i];
			os_rwlock_destroy(p);
		}
	}

	POBJ_FREE(&D_RW(mb->root)->locks);

	return 0;
}

/*
 * op_bench_rwlock -- lock and unlock the rwlock object
 *
 * If requested, increment the run_id of the memory pool.  In case of PMEMrwlock
 * this will force the rwlock object(s) reinitialization at the lock operation.
 */
static int
op_bench_rwlock(struct mutex_bench *mb)
{
	if (!mb->pa->use_system_threads) {
		if (mb->lock_mode == OP_MODE_1BY1) {
			bench_operation_1by1(
				!mb->pa->use_rdlock
					? pmemobj_rwlock_wrlock_wrapper
					: pmemobj_rwlock_rdlock_wrapper,
				pmemobj_rwlock_unlock_wrapper, mb, mb->pop);
		} else {
			bench_operation_all_lock(
				!mb->pa->use_rdlock
					? pmemobj_rwlock_wrlock_wrapper
					: pmemobj_rwlock_rdlock_wrapper,
				pmemobj_rwlock_unlock_wrapper, mb, mb->pop);
		}
		if (mb->pa->run_id_increment)
			mb->pop->run_id += 2; /* must be a multiple of 2 */
	} else {
		if (mb->lock_mode == OP_MODE_1BY1) {
			bench_operation_1by1(
				!mb->pa->use_rdlock ? os_rwlock_wrlock_wrapper
						    : os_rwlock_rdlock_wrapper,
				os_rwlock_unlock_wrapper, mb, nullptr);
		} else {
			bench_operation_all_lock(
				!mb->pa->use_rdlock ? os_rwlock_wrlock_wrapper
						    : os_rwlock_rdlock_wrapper,
				os_rwlock_unlock_wrapper, mb, nullptr);
		}
	}
	return 0;
}

/*
 * init_bench_vmutex -- allocate and initialize mutexes
 */
static int
init_bench_vmutex(struct mutex_bench *mb)
{
	struct my_root *root = D_RW(mb->root);
	assert(root != nullptr);

	POBJ_ZALLOC(mb->pop, &root->locks, lock_t,
		    mb->pa->n_locks * sizeof(lock_t));
	if (TOID_IS_NULL(root->locks)) {
		perror("POBJ_ZALLOC");
		return -1;
	}

	mb->locks = D_RW(root->locks);
	assert(mb->locks != nullptr);

	/* initialize PMEM volatile mutexes */
	for (unsigned i = 0; i < mb->pa->n_locks; i++) {
		auto *p = (PMEM_volatile_mutex *)&mb->locks[i];
		p->volatile_pmemmutex.runid = mb->pa->runid_initial_value;
		volatile_mutex_init(&p->volatile_pmemmutex.mutexp, nullptr);
	}

	return 0;
}

/*
 * exit_bench_vmutex -- destroy the mutex objects and release their
 * memory
 */
static int
exit_bench_vmutex(struct mutex_bench *mb)
{
	for (unsigned i = 0; i < mb->pa->n_locks; i++) {
		auto *p = (PMEM_volatile_mutex *)&mb->locks[i];
		volatile_mutex_destroy(mb->pop, p);
	}

	POBJ_FREE(&D_RW(mb->root)->locks);

	return 0;
}

/*
 * op_bench_volatile_mutex -- lock and unlock the mutex object
 */
static int
op_bench_vmutex(struct mutex_bench *mb)
{
	if (mb->lock_mode == OP_MODE_1BY1) {
		bench_operation_1by1(volatile_mutex_lock_wrapper,
				     volatile_mutex_unlock_wrapper, mb,
				     mb->pop);
	} else {
		bench_operation_all_lock(volatile_mutex_lock_wrapper,
					 volatile_mutex_unlock_wrapper, mb,
					 mb->pop);
	}

	if (mb->pa->run_id_increment)
		mb->pop->run_id += 2; /* must be a multiple of 2 */

	return 0;
}

struct bench_ops benchmark_ops[BENCH_MODE_MAX] = {
	{init_bench_mutex, exit_bench_mutex, op_bench_mutex},
	{init_bench_rwlock, exit_bench_rwlock, op_bench_rwlock},
	{init_bench_vmutex, exit_bench_vmutex, op_bench_vmutex}};

/*
 * operation_mode -- parses command line "--mode" and returns
 * proper operation mode
 */
static enum operation_mode
parse_op_mode(const char *arg)
{
	if (strcmp(arg, "1by1") == 0)
		return OP_MODE_1BY1;
	else if (strcmp(arg, "all-lock") == 0)
		return OP_MODE_ALL_LOCK;
	else
		return OP_MODE_MAX;
}

/*
 * benchmark_mode -- parses command line "--bench_type" and returns
 * proper benchmark ops
 */
static struct bench_ops *
parse_benchmark_mode(const char *arg)
{
	if (strcmp(arg, "mutex") == 0)
		return &benchmark_ops[BENCH_MODE_MUTEX];
	else if (strcmp(arg, "rwlock") == 0)
		return &benchmark_ops[BENCH_MODE_RWLOCK];
	else if (strcmp(arg, "volatile-mutex") == 0)
		return &benchmark_ops[BENCH_MODE_VOLATILE_MUTEX];
	else
		return nullptr;
}

/*
 * locks_init -- allocates persistent memory, maps it, creates the appropriate
 * objects in the allocated memory and initializes them
 */
static int
locks_init(struct benchmark *bench, struct benchmark_args *args)
{
	assert(bench != nullptr);
	assert(args != nullptr);

	enum file_type type = util_file_get_type(args->fname);
	if (type == OTHER_ERROR) {
		fprintf(stderr, "could not check type of file %s\n",
			args->fname);
		return -1;
	}

	int ret = 0;
	size_t poolsize;

	struct mutex_bench *mb = (struct mutex_bench *)malloc(sizeof(*mb));
	if (mb == nullptr) {
		perror("malloc");
		return -1;
	}

	mb->pa = (struct prog_args *)args->opts;

	mb->lock_mode = parse_op_mode(mb->pa->lock_mode);
	if (mb->lock_mode >= OP_MODE_MAX) {
		fprintf(stderr, "Invalid mutex mode: %s\n", mb->pa->lock_mode);
		errno = EINVAL;
		goto err_free_mb;
	}

	mb->ops = parse_benchmark_mode(mb->pa->lock_type);
	if (mb->ops == nullptr) {
		fprintf(stderr, "Invalid benchmark type: %s\n",
			mb->pa->lock_type);
		errno = EINVAL;
		goto err_free_mb;
	}

	/* reserve some space for metadata */
	poolsize = mb->pa->n_locks * sizeof(lock_t) + PMEMOBJ_MIN_POOL;

	if (args->is_poolset || type == TYPE_DEVDAX) {
		if (args->fsize < poolsize) {
			fprintf(stderr, "file size too large\n");
			goto err_free_mb;
		}

		poolsize = 0;
	}

	mb->pop = pmemobj_create(args->fname,
				 POBJ_LAYOUT_NAME(pmembench_lock_layout),
				 poolsize, args->fmode);

	if (mb->pop == nullptr) {
		ret = -1;
		perror("pmemobj_create");
		goto err_free_mb;
	}

	mb->root = POBJ_ROOT(mb->pop, struct my_root);
	assert(!TOID_IS_NULL(mb->root));

	ret = mb->ops->bench_init(mb);
	if (ret != 0)
		goto err_free_pop;

	pmembench_set_priv(bench, mb);

	return 0;

err_free_pop:
	pmemobj_close(mb->pop);

err_free_mb:
	free(mb);
	return ret;
}

/*
 * locks_exit -- destroys allocated objects and release memory
 */
static int
locks_exit(struct benchmark *bench, struct benchmark_args *args)
{
	assert(bench != nullptr);
	assert(args != nullptr);

	auto *mb = (struct mutex_bench *)pmembench_get_priv(bench);
	assert(mb != nullptr);

	mb->ops->bench_exit(mb);

	pmemobj_close(mb->pop);
	free(mb);

	return 0;
}

/*
 * locks_op -- actual benchmark operation
 *
 * Performs lock and unlock as by the program arguments.
 */
static int
locks_op(struct benchmark *bench, struct operation_info *info)
{
	auto *mb = (struct mutex_bench *)pmembench_get_priv(bench);
	assert(mb != nullptr);
	assert(mb->pop != nullptr);
	assert(!TOID_IS_NULL(mb->root));
	assert(mb->locks != nullptr);
	assert(mb->lock_mode < OP_MODE_MAX);

	mb->ops->bench_op(mb);

	return 0;
}

/* structure to define command line arguments */
static struct benchmark_clo locks_clo[7];
static struct benchmark_info locks_info;
CONSTRUCTOR(pmem_locks_constructor)
void
pmem_locks_constructor(void)
{
	locks_clo[0].opt_short = 'p';
	locks_clo[0].opt_long = "use_system_threads";
	locks_clo[0].descr = "Use os_thread locks instead of PMEM, "
			     "does not matter for volatile mutex";
	locks_clo[0].def = "false";
	locks_clo[0].off =
		clo_field_offset(struct prog_args, use_system_threads);
	locks_clo[0].type = CLO_TYPE_FLAG;

	locks_clo[1].opt_short = 'm';
	locks_clo[1].opt_long = "numlocks";
	locks_clo[1].descr = "The number of lock objects used "
			     "for benchmark";
	locks_clo[1].def = "1";
	locks_clo[1].off = clo_field_offset(struct prog_args, n_locks);
	locks_clo[1].type = CLO_TYPE_UINT;
	locks_clo[1].type_uint.size = clo_field_size(struct prog_args, n_locks);
	locks_clo[1].type_uint.base = CLO_INT_BASE_DEC;
	locks_clo[1].type_uint.min = 1;
	locks_clo[1].type_uint.max = UINT_MAX;

	locks_clo[2].opt_short = 0;
	locks_clo[2].opt_long = "mode";
	locks_clo[2].descr = "Locking mode";
	locks_clo[2].type = CLO_TYPE_STR;
	locks_clo[2].off = clo_field_offset(struct prog_args, lock_mode);
	locks_clo[2].def = "1by1";

	locks_clo[3].opt_short = 'r';
	locks_clo[3].opt_long = "run_id";
	locks_clo[3].descr = "Increment the run_id of PMEM object "
			     "pool after each operation";
	locks_clo[3].def = "false";
	locks_clo[3].off = clo_field_offset(struct prog_args, run_id_increment);
	locks_clo[3].type = CLO_TYPE_FLAG;

	locks_clo[4].opt_short = 'i';
	locks_clo[4].opt_long = "run_id_init_val";
	locks_clo[4].descr = "Use this value for initializing the "
			     "run_id of each PMEMmutex object";
	locks_clo[4].def = "2";
	locks_clo[4].off =
		clo_field_offset(struct prog_args, runid_initial_value);
	locks_clo[4].type = CLO_TYPE_UINT;

	locks_clo[4].type_uint.size =
		clo_field_size(struct prog_args, runid_initial_value);
	locks_clo[4].type_uint.base = CLO_INT_BASE_DEC;
	locks_clo[4].type_uint.min = 0;
	locks_clo[4].type_uint.max = UINT64_MAX;

	locks_clo[5].opt_short = 'b';
	locks_clo[5].opt_long = "bench_type";
	locks_clo[5].descr = "The Benchmark type: mutex, "
			     "rwlock or volatile-mutex";
	locks_clo[5].type = CLO_TYPE_STR;
	locks_clo[5].off = clo_field_offset(struct prog_args, lock_type);
	locks_clo[5].def = "mutex";

	locks_clo[6].opt_short = 'R';
	locks_clo[6].opt_long = "rdlock";
	locks_clo[6].descr = "Select read over write lock, only "
			     "valid when lock_type is \"rwlock\"";
	locks_clo[6].type = CLO_TYPE_FLAG;
	locks_clo[6].off = clo_field_offset(struct prog_args, use_rdlock);

	locks_info.name = "obj_locks";
	locks_info.brief = "Benchmark for pmem locks operations";
	locks_info.init = locks_init;
	locks_info.exit = locks_exit;
	locks_info.multithread = false;
	locks_info.multiops = true;
	locks_info.operation = locks_op;
	locks_info.measure_time = true;
	locks_info.clos = locks_clo;
	locks_info.nclos = ARRAY_SIZE(locks_clo);
	locks_info.opts_size = sizeof(struct prog_args);
	locks_info.rm_file = true;
	locks_info.allow_poolset = true;
	REGISTER_BENCHMARK(locks_info);
};
