// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2018, Intel Corporation */
/*
 * map_bench.cpp -- benchmarks for: ctree, btree, rtree, rbtree, hashmap_atomic
 * and hashmap_tx from examples.
 */
#include <cassert>

#include "benchmark.hpp"
#include "file.h"
#include "os.h"
#include "os_thread.h"
#include "poolset_util.hpp"

#include "map.h"
#include "map_btree.h"
#include "map_ctree.h"
#include "map_hashmap_atomic.h"
#include "map_hashmap_rp.h"
#include "map_hashmap_tx.h"
#include "map_rbtree.h"
#include "map_rtree.h"

/* Values less than 3 is not suitable for current rtree implementation */
#define FACTOR 3
#define ALLOC_OVERHEAD 64

TOID_DECLARE_ROOT(struct root);

struct root {
	TOID(struct map) map;
};

#define OBJ_TYPE_NUM 1

#define swap(a, b)                                                             \
	do {                                                                   \
		__typeof__(a) _tmp = (a);                                      \
		(a) = (b);                                                     \
		(b) = _tmp;                                                    \
	} while (0)

/* Values less than 2048 is not suitable for current rtree implementation */
#define SIZE_PER_KEY 2048

static const struct {
	const char *str;
	const struct map_ops *ops;
} map_types[] = {
	{"ctree", MAP_CTREE},		{"btree", MAP_BTREE},
	{"rtree", MAP_RTREE},		{"rbtree", MAP_RBTREE},
	{"hashmap_tx", MAP_HASHMAP_TX}, {"hashmap_atomic", MAP_HASHMAP_ATOMIC},
	{"hashmap_rp", MAP_HASHMAP_RP}};

#define MAP_TYPES_NUM (sizeof(map_types) / sizeof(map_types[0]))

struct map_bench_args {
	unsigned seed;
	uint64_t max_key;
	char *type;
	bool ext_tx;
	bool alloc;
};

struct map_bench_worker {
	uint64_t *keys;
	size_t nkeys;
};

struct map_bench {
	struct map_ctx *mapc;
	os_mutex_t lock;
	PMEMobjpool *pop;
	size_t pool_size;

	size_t nkeys;
	size_t init_nkeys;
	uint64_t *keys;
	struct benchmark_args *args;
	struct map_bench_args *margs;

	TOID(struct root) root;
	PMEMoid root_oid;
	TOID(struct map) map;

	int (*insert)(struct map_bench *, uint64_t);
	int (*remove)(struct map_bench *, uint64_t);
	int (*get)(struct map_bench *, uint64_t);
};

/*
 * mutex_lock_nofail -- locks mutex and aborts if locking failed
 */
static void
mutex_lock_nofail(os_mutex_t *lock)
{
	errno = os_mutex_lock(lock);
	if (errno) {
		perror("os_mutex_lock");
		abort();
	}
}

/*
 * mutex_unlock_nofail -- unlocks mutex and aborts if unlocking failed
 */
static void
mutex_unlock_nofail(os_mutex_t *lock)
{
	errno = os_mutex_unlock(lock);
	if (errno) {
		perror("os_mutex_unlock");
		abort();
	}
}

/*
 * get_key -- return 64-bit random key
 */
static uint64_t
get_key(unsigned *seed, uint64_t max_key)
{
	unsigned key_lo = os_rand_r(seed);
	unsigned key_hi = os_rand_r(seed);
	uint64_t key = (((uint64_t)key_hi) << 32) | ((uint64_t)key_lo);

	if (max_key)
		key = key % max_key;

	return key;
}

/*
 * parse_map_type -- parse type of map
 */
static const struct map_ops *
parse_map_type(const char *str)
{
	for (unsigned i = 0; i < MAP_TYPES_NUM; i++) {
		if (strcmp(str, map_types[i].str) == 0)
			return map_types[i].ops;
	}

	return nullptr;
}

/*
 * map_remove_free_op -- remove and free object from map
 */
static int
map_remove_free_op(struct map_bench *map_bench, uint64_t key)
{
	volatile int ret = 0;
	TX_BEGIN(map_bench->pop)
	{
		PMEMoid val = map_remove(map_bench->mapc, map_bench->map, key);
		if (OID_IS_NULL(val))
			ret = -1;
		else
			pmemobj_tx_free(val);
	}
	TX_ONABORT
	{
		ret = -1;
	}
	TX_END

	return ret;
}

/*
 * map_remove_root_op -- remove root object from map
 */
static int
map_remove_root_op(struct map_bench *map_bench, uint64_t key)
{
	PMEMoid val = map_remove(map_bench->mapc, map_bench->map, key);

	return !OID_EQUALS(val, map_bench->root_oid);
}

/*
 * map_remove_op -- main operation for map_remove benchmark
 */
static int
map_remove_op(struct benchmark *bench, struct operation_info *info)
{
	auto *map_bench = (struct map_bench *)pmembench_get_priv(bench);
	auto *tworker = (struct map_bench_worker *)info->worker->priv;

	uint64_t key = tworker->keys[info->index];

	mutex_lock_nofail(&map_bench->lock);

	int ret = map_bench->remove(map_bench, key);

	mutex_unlock_nofail(&map_bench->lock);

	return ret;
}

/*
 * map_insert_alloc_op -- allocate an object and insert to map
 */
static int
map_insert_alloc_op(struct map_bench *map_bench, uint64_t key)
{
	int ret = 0;

	TX_BEGIN(map_bench->pop)
	{
		PMEMoid oid =
			pmemobj_tx_alloc(map_bench->args->dsize, OBJ_TYPE_NUM);
		ret = map_insert(map_bench->mapc, map_bench->map, key, oid);
	}
	TX_ONABORT
	{
		ret = -1;
	}
	TX_END

	return ret;
}

/*
 * map_insert_root_op -- insert root object to map
 */
static int
map_insert_root_op(struct map_bench *map_bench, uint64_t key)
{
	return map_insert(map_bench->mapc, map_bench->map, key,
			  map_bench->root_oid);
}

/*
 * map_insert_op -- main operation for map_insert benchmark
 */
static int
map_insert_op(struct benchmark *bench, struct operation_info *info)
{
	auto *map_bench = (struct map_bench *)pmembench_get_priv(bench);
	auto *tworker = (struct map_bench_worker *)info->worker->priv;
	uint64_t key = tworker->keys[info->index];

	mutex_lock_nofail(&map_bench->lock);

	int ret = map_bench->insert(map_bench, key);

	mutex_unlock_nofail(&map_bench->lock);

	return ret;
}

/*
 * map_get_obj_op -- get object from map at specified key
 */
static int
map_get_obj_op(struct map_bench *map_bench, uint64_t key)
{
	PMEMoid val = map_get(map_bench->mapc, map_bench->map, key);

	return OID_IS_NULL(val);
}

/*
 * map_get_root_op -- get root object from map at specified key
 */
static int
map_get_root_op(struct map_bench *map_bench, uint64_t key)
{
	PMEMoid val = map_get(map_bench->mapc, map_bench->map, key);

	return !OID_EQUALS(val, map_bench->root_oid);
}

/*
 * map_get_op -- main operation for map_get benchmark
 */
static int
map_get_op(struct benchmark *bench, struct operation_info *info)
{
	auto *map_bench = (struct map_bench *)pmembench_get_priv(bench);
	auto *tworker = (struct map_bench_worker *)info->worker->priv;

	uint64_t key = tworker->keys[info->index];

	mutex_lock_nofail(&map_bench->lock);

	int ret = map_bench->get(map_bench, key);

	mutex_unlock_nofail(&map_bench->lock);

	return ret;
}

/*
 * map_common_init_worker -- common init worker function for map_* benchmarks
 */
static int
map_common_init_worker(struct benchmark *bench, struct benchmark_args *args,
		       struct worker_info *worker)
{
	struct map_bench_worker *tworker =
		(struct map_bench_worker *)calloc(1, sizeof(*tworker));
	struct map_bench *tree;
	struct map_bench_args *targs;

	if (!tworker) {
		perror("calloc");
		return -1;
	}

	tworker->nkeys = args->n_ops_per_thread;
	tworker->keys =
		(uint64_t *)malloc(tworker->nkeys * sizeof(*tworker->keys));

	if (!tworker->keys) {
		perror("malloc");
		goto err_free_worker;
	}

	tree = (struct map_bench *)pmembench_get_priv(bench);
	targs = (struct map_bench_args *)args->opts;
	if (targs->ext_tx) {
		int ret = pmemobj_tx_begin(tree->pop, nullptr);
		if (ret) {
			(void)pmemobj_tx_end();
			goto err_free_keys;
		}
	}

	worker->priv = tworker;

	return 0;
err_free_keys:
	free(tworker->keys);
err_free_worker:
	free(tworker);
	return -1;
}

/*
 * map_common_free_worker -- common cleanup worker function for map_*
 * benchmarks
 */
static void
map_common_free_worker(struct benchmark *bench, struct benchmark_args *args,
		       struct worker_info *worker)
{
	auto *tworker = (struct map_bench_worker *)worker->priv;
	auto *targs = (struct map_bench_args *)args->opts;

	if (targs->ext_tx) {
		pmemobj_tx_commit();
		(void)pmemobj_tx_end();
	}
	free(tworker->keys);
	free(tworker);
}

/*
 * map_insert_init_worker -- init worker function for map_insert benchmark
 */
static int
map_insert_init_worker(struct benchmark *bench, struct benchmark_args *args,
		       struct worker_info *worker)
{
	int ret = map_common_init_worker(bench, args, worker);
	if (ret)
		return ret;

	auto *targs = (struct map_bench_args *)args->opts;
	assert(targs);
	auto *tworker = (struct map_bench_worker *)worker->priv;

	assert(tworker);

	for (size_t i = 0; i < tworker->nkeys; i++)
		tworker->keys[i] = get_key(&targs->seed, targs->max_key);

	return 0;
}

/*
 * map_global_rand_keys_init -- assign random keys from global keys array
 */
static int
map_global_rand_keys_init(struct benchmark *bench, struct benchmark_args *args,
			  struct worker_info *worker)
{

	auto *tree = (struct map_bench *)pmembench_get_priv(bench);
	assert(tree);
	auto *targs = (struct map_bench_args *)args->opts;
	assert(targs);
	auto *tworker = (struct map_bench_worker *)worker->priv;

	assert(tworker);
	assert(tree->init_nkeys);

	/*
	 * Assign random keys from global tree->keys array without repetitions.
	 */
	for (size_t i = 0; i < tworker->nkeys; i++) {
		uint64_t index = get_key(&targs->seed, tree->init_nkeys);
		tworker->keys[i] = tree->keys[index];
		swap(tree->keys[index], tree->keys[tree->init_nkeys - 1]);
		tree->init_nkeys--;
	}

	return 0;
}

/*
 * map_remove_init_worker -- init worker function for map_remove benchmark
 */
static int
map_remove_init_worker(struct benchmark *bench, struct benchmark_args *args,
		       struct worker_info *worker)
{
	int ret = map_common_init_worker(bench, args, worker);
	if (ret)
		return ret;

	ret = map_global_rand_keys_init(bench, args, worker);
	if (ret)
		goto err_common_free_worker;
	return 0;
err_common_free_worker:
	map_common_free_worker(bench, args, worker);
	return -1;
}

/*
 * map_bench_get_init_worker -- init worker function for map_get benchmark
 */
static int
map_bench_get_init_worker(struct benchmark *bench, struct benchmark_args *args,
			  struct worker_info *worker)
{
	int ret = map_common_init_worker(bench, args, worker);
	if (ret)
		return ret;

	ret = map_global_rand_keys_init(bench, args, worker);
	if (ret)
		goto err_common_free_worker;
	return 0;
err_common_free_worker:
	map_common_free_worker(bench, args, worker);
	return -1;
}

/*
 * map_common_init -- common init function for map_* benchmarks
 */
static int
map_common_init(struct benchmark *bench, struct benchmark_args *args)
{
	assert(bench);
	assert(args);
	assert(args->opts);

	char path[PATH_MAX];
	if (util_safe_strcpy(path, args->fname, sizeof(path)) != 0)
		return -1;

	enum file_type type = util_file_get_type(args->fname);
	if (type == OTHER_ERROR) {
		fprintf(stderr, "could not check type of file %s\n",
			args->fname);
		return -1;
	}

	size_t size_per_key;
	struct map_bench *map_bench =
		(struct map_bench *)calloc(1, sizeof(*map_bench));

	if (!map_bench) {
		perror("calloc");
		return -1;
	}

	map_bench->args = args;
	map_bench->margs = (struct map_bench_args *)args->opts;

	const struct map_ops *ops = parse_map_type(map_bench->margs->type);
	if (!ops) {
		fprintf(stderr, "invalid map type value specified -- '%s'\n",
			map_bench->margs->type);
		goto err_free_bench;
	}

	if (map_bench->margs->ext_tx && args->n_threads > 1) {
		fprintf(stderr,
			"external transaction requires single thread\n");
		goto err_free_bench;
	}

	if (map_bench->margs->alloc) {
		map_bench->insert = map_insert_alloc_op;
		map_bench->remove = map_remove_free_op;
		map_bench->get = map_get_obj_op;
	} else {
		map_bench->insert = map_insert_root_op;
		map_bench->remove = map_remove_root_op;
		map_bench->get = map_get_root_op;
	}

	map_bench->nkeys = args->n_threads * args->n_ops_per_thread;
	map_bench->init_nkeys = map_bench->nkeys;
	size_per_key = map_bench->margs->alloc
		? SIZE_PER_KEY + map_bench->args->dsize + ALLOC_OVERHEAD
		: SIZE_PER_KEY;

	map_bench->pool_size = map_bench->nkeys * size_per_key * FACTOR;

	if (args->is_poolset || type == TYPE_DEVDAX) {
		if (args->fsize < map_bench->pool_size) {
			fprintf(stderr, "file size too large\n");
			goto err_free_bench;
		}

		map_bench->pool_size = 0;
	} else if (map_bench->pool_size < 2 * PMEMOBJ_MIN_POOL) {
		map_bench->pool_size = 2 * PMEMOBJ_MIN_POOL;
	}

	if (args->is_dynamic_poolset) {
		int ret = dynamic_poolset_create(args->fname,
						 map_bench->pool_size);
		if (ret == -1)
			goto err_free_bench;

		if (util_safe_strcpy(path, POOLSET_PATH, sizeof(path)) != 0)
			goto err_free_bench;

		map_bench->pool_size = 0;
	}

	map_bench->pop = pmemobj_create(path, "map_bench", map_bench->pool_size,
					args->fmode);
	if (!map_bench->pop) {
		fprintf(stderr, "pmemobj_create: %s\n", pmemobj_errormsg());
		goto err_free_bench;
	}

	errno = os_mutex_init(&map_bench->lock);
	if (errno) {
		perror("os_mutex_init");
		goto err_close;
	}

	map_bench->mapc = map_ctx_init(ops, map_bench->pop);
	if (!map_bench->mapc) {
		perror("map_ctx_init");
		goto err_destroy_lock;
	}

	map_bench->root = POBJ_ROOT(map_bench->pop, struct root);
	if (TOID_IS_NULL(map_bench->root)) {
		fprintf(stderr, "pmemobj_root: %s\n", pmemobj_errormsg());
		goto err_free_map;
	}

	map_bench->root_oid = map_bench->root.oid;

	if (map_create(map_bench->mapc, &D_RW(map_bench->root)->map, nullptr)) {
		perror("map_new");
		goto err_free_map;
	}

	map_bench->map = D_RO(map_bench->root)->map;

	pmembench_set_priv(bench, map_bench);
	return 0;
err_free_map:
	map_ctx_free(map_bench->mapc);
err_destroy_lock:
	os_mutex_destroy(&map_bench->lock);
err_close:
	pmemobj_close(map_bench->pop);
err_free_bench:
	free(map_bench);
	return -1;
}

/*
 * map_common_exit -- common cleanup function for map_* benchmarks
 */
static int
map_common_exit(struct benchmark *bench, struct benchmark_args *args)
{
	auto *tree = (struct map_bench *)pmembench_get_priv(bench);

	os_mutex_destroy(&tree->lock);
	map_ctx_free(tree->mapc);
	pmemobj_close(tree->pop);
	free(tree);
	return 0;
}

/*
 * map_keys_init -- initialize array with keys
 */
static int
map_keys_init(struct benchmark *bench, struct benchmark_args *args)
{
	auto *map_bench = (struct map_bench *)pmembench_get_priv(bench);
	assert(map_bench);
	auto *targs = (struct map_bench_args *)args->opts;
	assert(targs);

	assert(map_bench->nkeys != 0);
	map_bench->keys =
		(uint64_t *)malloc(map_bench->nkeys * sizeof(*map_bench->keys));

	if (!map_bench->keys) {
		perror("malloc");
		return -1;
	}

	int ret = 0;

	mutex_lock_nofail(&map_bench->lock);

	TX_BEGIN(map_bench->pop)
	{
		for (size_t i = 0; i < map_bench->nkeys; i++) {
			uint64_t key;
			PMEMoid oid;
			do {
				key = get_key(&targs->seed, targs->max_key);
				oid = map_get(map_bench->mapc, map_bench->map,
					      key);
			} while (!OID_IS_NULL(oid));

			if (targs->alloc)
				oid = pmemobj_tx_alloc(args->dsize,
						       OBJ_TYPE_NUM);
			else
				oid = map_bench->root_oid;

			ret = map_insert(map_bench->mapc, map_bench->map, key,
					 oid);
			if (ret)
				break;

			map_bench->keys[i] = key;
		}
	}
	TX_ONABORT
	{
		ret = -1;
	}
	TX_END

	mutex_unlock_nofail(&map_bench->lock);

	if (!ret)
		return 0;

	free(map_bench->keys);
	return ret;
}

/*
 * map_keys_exit -- cleanup of keys array
 */
static int
map_keys_exit(struct benchmark *bench, struct benchmark_args *args)
{
	auto *tree = (struct map_bench *)pmembench_get_priv(bench);
	free(tree->keys);
	return 0;
}

/*
 * map_remove_init -- init function for map_remove benchmark
 */
static int
map_remove_init(struct benchmark *bench, struct benchmark_args *args)
{
	int ret = map_common_init(bench, args);
	if (ret)
		return ret;
	ret = map_keys_init(bench, args);
	if (ret)
		goto err_exit_common;

	return 0;
err_exit_common:
	map_common_exit(bench, args);
	return -1;
}

/*
 * map_remove_exit -- cleanup function for map_remove benchmark
 */
static int
map_remove_exit(struct benchmark *bench, struct benchmark_args *args)
{
	map_keys_exit(bench, args);
	return map_common_exit(bench, args);
}

/*
 * map_bench_get_init -- init function for map_get benchmark
 */
static int
map_bench_get_init(struct benchmark *bench, struct benchmark_args *args)
{
	int ret = map_common_init(bench, args);
	if (ret)
		return ret;
	ret = map_keys_init(bench, args);
	if (ret)
		goto err_exit_common;

	return 0;
err_exit_common:
	map_common_exit(bench, args);
	return -1;
}

/*
 * map_get_exit -- exit function for map_get benchmark
 */
static int
map_get_exit(struct benchmark *bench, struct benchmark_args *args)
{
	map_keys_exit(bench, args);
	return map_common_exit(bench, args);
}

static struct benchmark_clo map_bench_clos[5];

static struct benchmark_info map_insert_info;
static struct benchmark_info map_remove_info;
static struct benchmark_info map_get_info;

CONSTRUCTOR(map_bench_constructor)
void
map_bench_constructor(void)
{
	map_bench_clos[0].opt_short = 'T';
	map_bench_clos[0].opt_long = "type";
	map_bench_clos[0].descr =
		"Type of container "
		"[ctree|btree|rtree|rbtree|hashmap_tx|hashmap_atomic]";

	map_bench_clos[0].off = clo_field_offset(struct map_bench_args, type);
	map_bench_clos[0].type = CLO_TYPE_STR;
	map_bench_clos[0].def = "ctree";

	map_bench_clos[1].opt_short = 's';
	map_bench_clos[1].opt_long = "seed";
	map_bench_clos[1].descr = "PRNG seed";
	map_bench_clos[1].off = clo_field_offset(struct map_bench_args, seed);
	map_bench_clos[1].type = CLO_TYPE_UINT;
	map_bench_clos[1].def = "1";
	map_bench_clos[1].type_uint.size =
		clo_field_size(struct map_bench_args, seed);
	map_bench_clos[1].type_uint.base = CLO_INT_BASE_DEC;
	map_bench_clos[1].type_uint.min = 1;
	map_bench_clos[1].type_uint.max = UINT_MAX;

	map_bench_clos[2].opt_short = 'M';
	map_bench_clos[2].opt_long = "max-key";
	map_bench_clos[2].descr = "maximum key (0 means no limit)";
	map_bench_clos[2].off =
		clo_field_offset(struct map_bench_args, max_key);
	map_bench_clos[2].type = CLO_TYPE_UINT;
	map_bench_clos[2].def = "0";
	map_bench_clos[2].type_uint.size =
		clo_field_size(struct map_bench_args, seed);
	map_bench_clos[2].type_uint.base = CLO_INT_BASE_DEC;
	map_bench_clos[2].type_uint.min = 0;
	map_bench_clos[2].type_uint.max = UINT64_MAX;

	map_bench_clos[3].opt_short = 'x';
	map_bench_clos[3].opt_long = "external-tx";
	map_bench_clos[3].descr = "Use external transaction for all "
				  "operations (works with single "
				  "thread only)";
	map_bench_clos[3].off = clo_field_offset(struct map_bench_args, ext_tx);
	map_bench_clos[3].type = CLO_TYPE_FLAG;

	map_bench_clos[4].opt_short = 'A';
	map_bench_clos[4].opt_long = "alloc";
	map_bench_clos[4].descr = "Allocate object of specified size "
				  "when inserting";
	map_bench_clos[4].off = clo_field_offset(struct map_bench_args, alloc);
	map_bench_clos[4].type = CLO_TYPE_FLAG;

	map_insert_info.name = "map_insert";
	map_insert_info.brief = "Inserting to tree map";
	map_insert_info.init = map_common_init;
	map_insert_info.exit = map_common_exit;
	map_insert_info.multithread = true;
	map_insert_info.multiops = true;
	map_insert_info.init_worker = map_insert_init_worker;
	map_insert_info.free_worker = map_common_free_worker;
	map_insert_info.operation = map_insert_op;
	map_insert_info.measure_time = true;
	map_insert_info.clos = map_bench_clos;
	map_insert_info.nclos = ARRAY_SIZE(map_bench_clos);
	map_insert_info.opts_size = sizeof(struct map_bench_args);
	map_insert_info.rm_file = true;
	map_insert_info.allow_poolset = true;
	REGISTER_BENCHMARK(map_insert_info);

	map_remove_info.name = "map_remove";
	map_remove_info.brief = "Removing from tree map";
	map_remove_info.init = map_remove_init;
	map_remove_info.exit = map_remove_exit;
	map_remove_info.multithread = true;
	map_remove_info.multiops = true;
	map_remove_info.init_worker = map_remove_init_worker;
	map_remove_info.free_worker = map_common_free_worker;
	map_remove_info.operation = map_remove_op;
	map_remove_info.measure_time = true;
	map_remove_info.clos = map_bench_clos;
	map_remove_info.nclos = ARRAY_SIZE(map_bench_clos);
	map_remove_info.opts_size = sizeof(struct map_bench_args);
	map_remove_info.rm_file = true;
	map_remove_info.allow_poolset = true;
	REGISTER_BENCHMARK(map_remove_info);

	map_get_info.name = "map_get";
	map_get_info.brief = "Tree lookup";
	map_get_info.init = map_bench_get_init;
	map_get_info.exit = map_get_exit;
	map_get_info.multithread = true;
	map_get_info.multiops = true;
	map_get_info.init_worker = map_bench_get_init_worker;
	map_get_info.free_worker = map_common_free_worker;
	map_get_info.operation = map_get_op;
	map_get_info.measure_time = true;
	map_get_info.clos = map_bench_clos;
	map_get_info.nclos = ARRAY_SIZE(map_bench_clos);
	map_get_info.opts_size = sizeof(struct map_bench_args);
	map_get_info.rm_file = true;
	map_get_info.allow_poolset = true;
	REGISTER_BENCHMARK(map_get_info);
}
