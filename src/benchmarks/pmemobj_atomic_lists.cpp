// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2019, Intel Corporation */

/*
 * pmemobj_atomic_lists.cpp -- benchmark for pmemobj atomic list API
 */

#include "benchmark.hpp"
#include "file.h"
#include "libpmemobj.h"
#include "queue.h"
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#define FACTOR 8
#define LAYOUT_NAME "benchmark"

struct obj_bench;
struct obj_worker;
struct element;

TOID_DECLARE(struct item, 0);
TOID_DECLARE(struct list, 1);

typedef size_t (*fn_type_num_t)(size_t worker_idx, size_t op_idx);

typedef struct element (*fn_position_t)(struct obj_worker *obj_worker,
					size_t op_idx);

typedef int (*fn_init_t)(struct worker_info *worker, size_t n_elm,
			 size_t list_len);
/*
 * args -- stores command line parsed arguments.
 */
struct obj_list_args {
	char *type_num;    /* type_number mode - one, per-thread, rand */
	char *position;    /* position - head, tail, middle, rand */
	unsigned list_len; /* initial list length */
	bool queue;	/* use circle queue from <sys/queue.h> */
	bool range;	/* use random allocation size */
	unsigned min_size; /* minimum random allocation size */
	unsigned seed;     /* seed value */
};

/*
 * obj_bench -- stores variables used in benchmark, passed within functions.
 */
static struct obj_bench {

	/* handle to persistent pool */
	PMEMobjpool *pop;

	/* pointer to benchmark specific arguments */
	struct obj_list_args *args;

	/* array to store random type_number values */
	size_t *random_types;

	/*
	 * fn_rpositions array stores random functions returning proper element
	 * from list, if position where operation is performed is random.
	 * Possible function which can be in array are:
	 *	- position_head,
	 *	- position_tail,
	 *	- position_middle.
	 */
	size_t *alloc_sizes; /* array to store random sizes of each object */
	size_t max_len;      /* maximum list length */
	size_t min_len;      /* initial list length */
	int type_mode;       /* type_number mode */
	int position_mode;   /* list destination mode */

	/*
	 * fn_type_num gets proper function assigned, depending on the
	 * value of the type_mode argument, which returns proper type number for
	 * each persistent object. Possible functions are:
	 *	- type_mode_one,
	 *	- type_mode_per_thread,
	 *	- type_mode_rand.
	 */
	fn_type_num_t fn_type_num;

	/*
	 * fn_position gets proper function assigned, depending  on the value
	 * of the position argument, which returns handle to proper element on
	 * the list. Possible functions are:
	 *	- position_head,
	 *	- position_tail,
	 *	- position_middle,
	 *	- position_rand.
	 */
	fn_position_t fn_position;

	/*
	 * fn_init gets proper function assigned, depending on the file_io
	 * flag, which allocates objects and initializes proper list. Possible
	 * functions are:
	 *	- obj_init_list,
	 *	- queue_init_list.
	 */
	fn_init_t fn_init;
} obj_bench;

/*
 * item -- structure used to connect elements in lists.
 */
struct item {
	POBJ_LIST_ENTRY(struct item) field;
	PMDK_CIRCLEQ_ENTRY(item) fieldq;
};

/*
 * element -- struct contains one item from list with proper type.
 */
struct element {
	struct item *itemq;
	TOID(struct item) itemp;
	bool before;
};

/*
 * obj_worker -- stores variables used by one thread, concerning one list.
 */
struct obj_worker {

	/* head of the pmemobj list */
	POBJ_LIST_HEAD(plist, struct item) head;

	/* head of the circular queue */
	PMDK_CIRCLEQ_HEAD(qlist, item) headq;
	TOID(struct item) * oids;    /* persistent pmemobj list elements */
	struct item **items;	 /* volatile elements */
	size_t n_elm;		     /* number of elements in array */
	fn_position_t *fn_positions; /* element access functions */
	struct element elm;	  /* pointer to current element */
	/*
	 * list_move is a pointer to structure storing variables used by
	 * second list (used only for obj_move benchmark).
	 */
	struct obj_worker *list_move;
};

/*
 * position_mode -- list destination type
 */
enum position_mode {
	/* object inserted/removed/moved to/from head of list */
	POSITION_MODE_HEAD,

	/* object inserted/removed/moved to/from tail of list */
	POSITION_MODE_TAIL,

	/*
	 * object inserted/removed/moved to/from second element of the list
	 * or to/from head if list length equal to one
	 */
	POSITION_MODE_MIDDLE,

	/* object inserted/removed/moved to/from head, tail or middle */
	POSITION_MODE_RAND,
	POSITION_MODE_UNKNOWN,
};

/*
 * type_mode -- type number type
 */
enum type_mode {
	TYPE_MODE_ONE, /* one type number for all of objects */

	/* one type number for objects allocated by the same thread */
	TYPE_MODE_PER_THREAD,
	TYPE_MODE_RAND, /* random type number for each object */
	TYPE_MODE_UNKNOWN,
};

/*
 * position_head -- returns head of the persistent list or volatile queue.
 */
static struct element
position_head(struct obj_worker *obj_worker, size_t op_idx)
{
	struct element head = {nullptr, OID_NULL, false};
	head.before = true;
	if (!obj_bench.args->queue)
		head.itemp = POBJ_LIST_FIRST(&obj_worker->head);
	else
		head.itemq = PMDK_CIRCLEQ_FIRST(&obj_worker->headq);
	return head;
}

/*
 * position_tail -- returns tail of the persistent list or volatile queue.
 */
static struct element
position_tail(struct obj_worker *obj_worker, size_t op_idx)
{
	struct element tail = {nullptr, OID_NULL, false};
	tail.before = false;
	if (!obj_bench.args->queue)
		tail.itemp = POBJ_LIST_LAST(&obj_worker->head, field);
	else
		tail.itemq = PMDK_CIRCLEQ_LAST(&obj_worker->headq);
	return tail;
}

/*
 * position_middle -- returns second or first element from the persistent list
 * or volatile queue.
 */
static struct element
position_middle(struct obj_worker *obj_worker, size_t op_idx)
{
	struct element elm = position_head(obj_worker, op_idx);
	elm.before = true;
	if (!obj_bench.args->queue)
		elm.itemp = POBJ_LIST_NEXT(elm.itemp, field);
	else
		elm.itemq = PMDK_CIRCLEQ_NEXT(elm.itemq, fieldq);
	return elm;
}

/*
 * position_rand -- returns first, second or last element from the persistent
 * list or volatile queue based on r_positions array.
 */
static struct element
position_rand(struct obj_worker *obj_worker, size_t op_idx)
{
	struct element elm;
	elm = obj_worker->fn_positions[op_idx](obj_worker, op_idx);
	elm.before = true;
	return elm;
}

/*
 * type_mode_one -- always returns 0, as in the mode TYPE_MODE_ONE
 * all of the persistent objects have the same type_number value.
 */
static size_t
type_mode_one(size_t worker_idx, size_t op_idx)
{
	return 0;
}

/*
 * type_mode_per_thread -- always returns the index of the worker,
 * as in the TYPE_MODE_PER_THREAD the value of the persistent object
 * type_number is specific to the thread.
 */
static size_t
type_mode_per_thread(size_t worker_idx, size_t op_idx)
{
	return worker_idx;
}

/*
 * type_mode_rand -- returns the value from the random_types array assigned
 * for the specific operation in a specific thread.
 */
static size_t
type_mode_rand(size_t worker_idx, size_t op_idx)
{
	return obj_bench.random_types[op_idx];
}

const char *type_num_names[] = {"one", "per-thread", "rand"};
const char *position_names[] = {"head", "tail", "middle", "rand"};
static fn_type_num_t type_num_modes[] = {type_mode_one, type_mode_per_thread,
					 type_mode_rand};
static fn_position_t positions[] = {position_head, position_tail,
				    position_middle, position_rand};

/* function pointers randomly picked when using rand mode */
static fn_position_t rand_positions[] = {position_head, position_tail,
					 position_middle};

/*
 * get_item -- common part of initial operation of the all benchmarks.
 * It gets pointer to element on the list where object will
 * be inserted/removed/moved to/from.
 */
static void
get_item(struct benchmark *bench, struct operation_info *info)
{
	auto *obj_worker = (struct obj_worker *)info->worker->priv;
	obj_worker->elm = obj_bench.fn_position(obj_worker, info->index);
}

/*
 * get_move_item -- special part of initial operation of the obj_move
 * benchmarks. It gets pointer to element on the list where object will be
 * inserted/removed/moved to/from.
 */
static void
get_move_item(struct benchmark *bench, struct operation_info *info)
{
	auto *obj_worker = (struct obj_worker *)info->worker->priv;
	obj_worker->list_move->elm =
		obj_bench.fn_position(obj_worker->list_move, info->index);

	get_item(bench, info);
}

/*
 * parse_args -- parse command line string argument
 */
static int
parse_args(char *arg, int max, const char **names)
{
	int i = 0;
	for (; i < max && strcmp(names[i], arg) != 0; i++)
		;
	if (i == max)
		fprintf(stderr, "Invalid argument\n");
	return i;
}

/*
 * obj_init_list -- special part of worker initialization, performed only if
 * queue flag set false. Allocates proper number of items, and inserts proper
 * part of them to the pmemobj list.
 */
static int
obj_init_list(struct worker_info *worker, size_t n_oids, size_t list_len)
{
	size_t i;
	auto *obj_worker = (struct obj_worker *)worker->priv;
	obj_worker->oids =
		(TOID(struct item) *)calloc(n_oids, sizeof(TOID(struct item)));
	if (obj_worker->oids == nullptr) {
		perror("calloc");
		return -1;
	}
	for (i = 0; i < n_oids; i++) {
		size_t type_num = obj_bench.fn_type_num(worker->index, i);
		size_t size = obj_bench.alloc_sizes[i];
		auto *tmp = (PMEMoid *)&obj_worker->oids[i];
		if (pmemobj_alloc(obj_bench.pop, tmp, size, type_num, nullptr,
				  nullptr) != 0)
			goto err_oids;
	}
	for (i = 0; i < list_len; i++)
		POBJ_LIST_INSERT_TAIL(obj_bench.pop, &obj_worker->head,
				      obj_worker->oids[i], field);
	return 0;
err_oids:
	for (; i > 0; i--)
		POBJ_FREE(&obj_worker->oids[i - 1]);
	free(obj_worker->oids);
	return -1;
}

/*
 * queue_init_list -- special part of worker initialization, performed only if
 * queue flag set. Initiates circle queue, allocates proper number of items and
 * inserts proper part of them to the queue.
 */
static int
queue_init_list(struct worker_info *worker, size_t n_items, size_t list_len)
{
	size_t i;
	auto *obj_worker = (struct obj_worker *)worker->priv;
	PMDK_CIRCLEQ_INIT(&obj_worker->headq);
	obj_worker->items =
		(struct item **)malloc(n_items * sizeof(struct item *));
	if (obj_worker->items == nullptr) {
		perror("malloc");
		return -1;
	}

	for (i = 0; i < n_items; i++) {
		size_t size = obj_bench.alloc_sizes[i];
		obj_worker->items[i] = (struct item *)calloc(1, size);
		if (obj_worker->items[i] == nullptr) {
			perror("calloc");
			goto err;
		}
	}

	for (i = 0; i < list_len; i++)
		PMDK_CIRCLEQ_INSERT_TAIL(&obj_worker->headq,
					 obj_worker->items[i], fieldq);

	return 0;
err:
	for (; i > 0; i--)
		free(obj_worker->items[i - 1]);
	free(obj_worker->items);
	return -1;
}

/*
 * queue_free_worker_list -- special part for the worker de-initialization when
 * queue flag is true. Releases items directly from atomic list.
 */
static void
queue_free_worker_list(struct obj_worker *obj_worker)
{
	while (!PMDK_CIRCLEQ_EMPTY(&obj_worker->headq)) {
		struct item *tmp = PMDK_CIRCLEQ_LAST(&obj_worker->headq);
		PMDK_CIRCLEQ_REMOVE(&obj_worker->headq, tmp, fieldq);
		free(tmp);
	}
	free(obj_worker->items);
}

/*
 * obj_free_worker_list -- special part for the worker de-initialization when
 * queue flag is false. Releases items directly from atomic list.
 */
static void
obj_free_worker_list(struct obj_worker *obj_worker)
{
	while (!POBJ_LIST_EMPTY(&obj_worker->head)) {
		TOID(struct item) tmp = POBJ_LIST_FIRST(&obj_worker->head);
		POBJ_LIST_REMOVE_FREE(obj_bench.pop, &obj_worker->head, tmp,
				      field);
	}
	free(obj_worker->oids);
}

/*
 * obj_free_worker_items -- special part for the worker de-initialization when
 * queue flag is false. Releases items used for create pmemobj list.
 */
static void
obj_free_worker_items(struct obj_worker *obj_worker)
{
	for (size_t i = 0; i < obj_worker->n_elm; i++)
		POBJ_FREE(&obj_worker->oids[i]);
	free(obj_worker->oids);
}

/*
 * queue_free_worker_items -- special part for the worker de-initialization
 * when queue flag set. Releases used for create circle queue.
 */
static void
queue_free_worker_items(struct obj_worker *obj_worker)
{
	for (size_t i = 0; i < obj_worker->n_elm; i++)
		free(obj_worker->items[i]);
	free(obj_worker->items);
}

/*
 * random_positions -- allocates array and calculates random values for
 * defining positions where each operation will be performed. Used only
 * in POSITION_MODE_RAND
 */
static fn_position_t *
random_positions(void)
{
	auto *positions = (fn_position_t *)calloc(obj_bench.max_len,
						  sizeof(fn_position_t));
	if (positions == nullptr) {
		perror("calloc");
		return nullptr;
	}

	if (obj_bench.args->seed != 0)
		srand(obj_bench.args->seed);

	size_t rmax = ARRAY_SIZE(rand_positions);
	for (size_t i = 0; i < obj_bench.max_len; i++) {
		size_t id = RRAND(rmax, 0);
		positions[i] = rand_positions[id];
	}

	return positions;
}

/*
 * rand_values -- allocates array and if range mode calculates random
 * values as allocation sizes for each object otherwise populates whole array
 * with max value. Used only when range flag set.
 */
static size_t *
random_values(size_t min, size_t max, size_t n_ops, size_t min_range)
{
	auto *randoms = (size_t *)calloc(n_ops, sizeof(size_t));
	if (randoms == nullptr) {
		perror("calloc");
		return nullptr;
	}
	for (size_t i = 0; i < n_ops; i++)
		randoms[i] = max;
	if (min > min_range) {
		if (min > max) {
			fprintf(stderr, "Invalid size\n");
			free(randoms);
			return nullptr;
		}
		for (size_t i = 0; i < n_ops; i++)
			randoms[i] = RRAND(max, min);
	}
	return randoms;
}
/*
 * queue_insert_op -- main operations of the obj_insert benchmark when queue
 * flag set to true.
 */
static int
queue_insert_op(struct operation_info *info)
{
	auto *obj_worker = (struct obj_worker *)info->worker->priv;
	PMDK_CIRCLEQ_INSERT_AFTER(
		&obj_worker->headq, obj_worker->elm.itemq,
		obj_worker->items[info->index + obj_bench.min_len], fieldq);
	return 0;
}

/*
 * obj_insert_op -- main operations of the obj_insert benchmark when queue flag
 * set to false.
 */
static int
obj_insert_op(struct operation_info *info)
{
	auto *obj_worker = (struct obj_worker *)info->worker->priv;
	POBJ_LIST_INSERT_AFTER(
		obj_bench.pop, &obj_worker->head, obj_worker->elm.itemp,
		obj_worker->oids[info->index + obj_bench.min_len], field);
	return 0;
}

/*
 * queue_remove_op -- main operations of the obj_remove benchmark when queue
 * flag set to true.
 */
static int
queue_remove_op(struct operation_info *info)
{
	auto *obj_worker = (struct obj_worker *)info->worker->priv;
	PMDK_CIRCLEQ_REMOVE(&obj_worker->headq, obj_worker->elm.itemq, fieldq);
	return 0;
}

/*
 * obj_remove_op -- main operations of the obj_remove benchmark when queue flag
 * set to false.
 */
static int
obj_remove_op(struct operation_info *info)
{
	auto *obj_worker = (struct obj_worker *)info->worker->priv;
	POBJ_LIST_REMOVE(obj_bench.pop, &obj_worker->head,
			 obj_worker->elm.itemp, field);
	return 0;
}

/*
 * insert_op -- main operations of the obj_insert benchmark.
 */
static int
insert_op(struct benchmark *bench, struct operation_info *info)
{
	get_item(bench, info);

	return obj_bench.args->queue ? queue_insert_op(info)
				     : obj_insert_op(info);
}

/*
 * obj_insert_new_op -- main operations of the obj_insert_new benchmark.
 */
static int
obj_insert_new_op(struct benchmark *bench, struct operation_info *info)
{
	get_item(bench, info);

	auto *obj_worker = (struct obj_worker *)info->worker->priv;
	PMEMoid tmp;
	size_t size = obj_bench.alloc_sizes[info->index];
	size_t type_num =
		obj_bench.fn_type_num(info->worker->index, info->index);
	tmp = pmemobj_list_insert_new(
		obj_bench.pop, offsetof(struct item, field), &obj_worker->head,
		obj_worker->elm.itemp.oid, obj_worker->elm.before, size,
		type_num, nullptr, nullptr);

	if (OID_IS_NULL(tmp)) {
		perror("pmemobj_list_insert_new");
		return -1;
	}

	return 0;
}

/*
 * remove_op -- main operations of the obj_remove benchmark.
 */
static int
remove_op(struct benchmark *bench, struct operation_info *info)
{
	get_item(bench, info);

	return obj_bench.args->queue ? queue_remove_op(info)
				     : obj_remove_op(info);
}

/*
 * obj_remove_free_op -- main operation of the obj_remove_free benchmark.
 */
static int
obj_remove_free_op(struct benchmark *bench, struct operation_info *info)
{
	get_item(bench, info);

	auto *obj_worker = (struct obj_worker *)info->worker->priv;
	POBJ_LIST_REMOVE_FREE(obj_bench.pop, &obj_worker->head,
			      obj_worker->elm.itemp, field);
	return 0;
}

/*
 * obj_move_op -- main operation of the obj_move benchmark.
 */
static int
obj_move_op(struct benchmark *bench, struct operation_info *info)
{
	get_move_item(bench, info);

	auto *obj_worker = (struct obj_worker *)info->worker->priv;
	POBJ_LIST_MOVE_ELEMENT_BEFORE(obj_bench.pop, &obj_worker->head,
				      &obj_worker->list_move->head,
				      obj_worker->list_move->elm.itemp,
				      obj_worker->elm.itemp, field, field);
	return 0;
}

/*
 * free_worker -- free common worker state
 */
static void
free_worker(struct obj_worker *obj_worker)
{
	if (obj_bench.position_mode == POSITION_MODE_RAND)
		free(obj_worker->fn_positions);
	free(obj_worker);
}

/*
 * free_worker_list -- worker de-initialization function for: obj_insert_new,
 * obj_remove_free, obj_move. Requires releasing objects directly from list.
 */
static void
free_worker_list(struct benchmark *bench, struct benchmark_args *args,
		 struct worker_info *worker)
{
	auto *obj_worker = (struct obj_worker *)worker->priv;
	obj_bench.args->queue ? queue_free_worker_list(obj_worker)
			      : obj_free_worker_list(obj_worker);
	free_worker(obj_worker);
}

/*
 * obj_free_worker_items -- worker de-initialization function of obj_insert and
 * obj_remove benchmarks, where deallocation can't be performed directly on the
 * list and where is possibility of using queue flag.
 */
static void
free_worker_items(struct benchmark *bench, struct benchmark_args *args,
		  struct worker_info *worker)
{
	auto *obj_worker = (struct obj_worker *)worker->priv;
	auto *obj_args = (struct obj_list_args *)args->opts;
	obj_args->queue ? queue_free_worker_items(obj_worker)
			: obj_free_worker_items(obj_worker);
	free_worker(obj_worker);
}

/*
 * obj_move_free_worker -- special part for the worker de-initialization
 * function of obj_move benchmarks.
 */
static void
obj_move_free_worker(struct benchmark *bench, struct benchmark_args *args,
		     struct worker_info *worker)
{
	auto *obj_worker = (struct obj_worker *)worker->priv;
	while (!POBJ_LIST_EMPTY(&obj_worker->list_move->head))
		POBJ_LIST_REMOVE_FREE(
			obj_bench.pop, &obj_worker->list_move->head,
			POBJ_LIST_LAST(&obj_worker->list_move->head, field),
			field);

	if (obj_bench.position_mode == POSITION_MODE_RAND)
		free(obj_worker->list_move->fn_positions);
	free(obj_worker->list_move);
	free_worker_list(bench, args, worker);
}

/*
 * obj_init_worker -- common part for the worker initialization for:
 * obj_insert, obj_insert_new, obj_remove obj_remove_free and obj_move.
 */
static int
obj_init_worker(struct worker_info *worker, size_t n_elm, size_t list_len)
{
	auto *obj_worker =
		(struct obj_worker *)calloc(1, sizeof(struct obj_worker));
	if (obj_worker == nullptr) {
		perror("calloc");
		return -1;
	}

	worker->priv = obj_worker;
	obj_worker->n_elm = obj_bench.max_len;
	obj_worker->list_move = nullptr;
	if (obj_bench.position_mode == POSITION_MODE_RAND) {
		obj_worker->fn_positions = random_positions();
		if (obj_worker->fn_positions == nullptr)
			goto err;
	}
	if (obj_bench.fn_init(worker, n_elm, list_len) != 0)
		goto err_positions;

	return 0;
err_positions:
	free(obj_worker->fn_positions);
err:
	free(obj_worker);
	return -1;
}

/*
 * obj_insert_init_worker -- worker initialization functions of the obj_insert
 * benchmark.
 */
static int
obj_insert_init_worker(struct benchmark *bench, struct benchmark_args *args,
		       struct worker_info *worker)
{
	return obj_init_worker(worker, obj_bench.max_len, obj_bench.min_len);
}

/*
 * obj_insert_new_init_worker -- worker initialization functions of the
 * obj_insert_new benchmark.
 */
static int
obj_insert_new_init_worker(struct benchmark *bench, struct benchmark_args *args,
			   struct worker_info *worker)
{
	return obj_init_worker(worker, obj_bench.min_len, obj_bench.min_len);
}

/*
 * obj_remove_init_worker -- worker initialization functions of the obj_remove
 * and obj_remove_free benchmarks.
 */
static int
obj_remove_init_worker(struct benchmark *bench, struct benchmark_args *args,
		       struct worker_info *worker)
{
	return obj_init_worker(worker, obj_bench.max_len, obj_bench.max_len);
}

/*
 * obj_move_init_worker -- worker initialization functions of the obj_move
 * benchmark.
 */
static int
obj_move_init_worker(struct benchmark *bench, struct benchmark_args *args,
		     struct worker_info *worker)
{
	if (obj_init_worker(worker, obj_bench.max_len, obj_bench.max_len) != 0)
		return -1;

	auto *obj_worker = (struct obj_worker *)worker->priv;
	obj_worker->list_move =
		(struct obj_worker *)calloc(1, sizeof(struct obj_worker));
	if (obj_worker->list_move == nullptr) {
		perror("calloc");
		goto free;
	}
	size_t i;
	if (obj_bench.position_mode == POSITION_MODE_RAND) {
		obj_worker->list_move->fn_positions = random_positions();
		if (obj_worker->list_move->fn_positions == nullptr)
			goto free_list_move;
	}
	for (i = 0; i < obj_bench.min_len; i++) {
		size_t size = obj_bench.alloc_sizes[i];
		POBJ_LIST_INSERT_NEW_TAIL(obj_bench.pop,
					  &obj_worker->list_move->head, field,
					  size, nullptr, nullptr);
		if (TOID_IS_NULL(POBJ_LIST_LAST(&obj_worker->list_move->head,
						field))) {
			perror("pmemobj_list_insert_new");
			goto free_all;
		}
	}
	return 0;
free_all:
	for (; i > 0; i--) {
		POBJ_LIST_REMOVE_FREE(
			obj_bench.pop, &obj_worker->list_move->head,
			POBJ_LIST_LAST(&obj_worker->list_move->head, field),
			field);
	}
	free(obj_worker->list_move->fn_positions);
free_list_move:
	free(obj_worker->list_move);
free:
	free_worker_list(bench, args, worker);
	return -1;
}

/*
 * obj_init - common part of the benchmark initialization for: obj_insert,
 * obj_insert_new, obj_remove, obj_remove_free and obj_move used in their init
 * functions. Parses command line arguments, sets variables and
 * creates persistent pool.
 */
static int
obj_init(struct benchmark *bench, struct benchmark_args *args)
{
	assert(bench != nullptr);
	assert(args != nullptr);
	assert(args->opts != nullptr);

	enum file_type type = util_file_get_type(args->fname);
	if (type == OTHER_ERROR) {
		fprintf(stderr, "could not check type of file %s\n",
			args->fname);
		return -1;
	}

	obj_bench.args = (struct obj_list_args *)args->opts;
	obj_bench.min_len = obj_bench.args->list_len + 1;
	obj_bench.max_len = args->n_ops_per_thread + obj_bench.min_len;

	obj_bench.fn_init =
		obj_bench.args->queue ? queue_init_list : obj_init_list;
	/* Decide if use random or state allocation sizes */
	size_t obj_size = args->dsize < sizeof(struct item)
		? sizeof(struct item)
		: args->dsize;
	size_t min_size = obj_bench.args->min_size < sizeof(struct item)
		? sizeof(struct item)
		: obj_bench.args->min_size;
	obj_bench.alloc_sizes = random_values(
		min_size, obj_size, obj_bench.max_len, sizeof(struct item));
	if (obj_bench.alloc_sizes == nullptr)
		goto free_random_types;

	/* Decide where operations will be performed */
	obj_bench.position_mode =
		parse_args(obj_bench.args->position, POSITION_MODE_UNKNOWN,
			   position_names);
	if (obj_bench.position_mode == POSITION_MODE_UNKNOWN)
		goto free_all;

	obj_bench.fn_position = positions[obj_bench.position_mode];
	if (!obj_bench.args->queue) {
		/* Decide what type number will be used */
		obj_bench.type_mode =
			parse_args(obj_bench.args->type_num, TYPE_MODE_UNKNOWN,
				   type_num_names);
		if (obj_bench.type_mode == TYPE_MODE_UNKNOWN)
			return -1;

		obj_bench.fn_type_num = type_num_modes[obj_bench.type_mode];
		if (obj_bench.type_mode == TYPE_MODE_RAND) {
			obj_bench.random_types = random_values(
				1, UINT32_MAX, obj_bench.max_len, 0);
			if (obj_bench.random_types == nullptr)
				return -1;
		}
		/*
		 * Multiplication by FACTOR prevents from out of memory error
		 * as the actual size of the allocated persistent objects
		 * is always larger than requested.
		 */
		size_t psize =
			(args->n_ops_per_thread + obj_bench.min_len + 1) *
			obj_size * args->n_threads * FACTOR;
		if (args->is_poolset || type == TYPE_DEVDAX) {
			if (args->fsize < psize) {
				fprintf(stderr, "file size too large\n");
				goto free_all;
			}

			psize = 0;
		} else if (psize < PMEMOBJ_MIN_POOL) {
			psize = PMEMOBJ_MIN_POOL;
		}

		/* Create pmemobj pool. */
		if ((obj_bench.pop = pmemobj_create(args->fname, LAYOUT_NAME,
						    psize, args->fmode)) ==
		    nullptr) {
			perror(pmemobj_errormsg());
			goto free_all;
		}
	}
	return 0;
free_all:
	free(obj_bench.alloc_sizes);
free_random_types:
	if (obj_bench.type_mode == TYPE_MODE_RAND)
		free(obj_bench.random_types);
	return -1;
}

/*
 * obj_exit -- common part for the exit function for: obj_insert,
 * obj_insert_new, obj_remove, obj_remove_free and obj_move used in their exit
 * functions.
 */
static int
obj_exit(struct benchmark *bench, struct benchmark_args *args)
{
	if (!obj_bench.args->queue) {
		pmemobj_close(obj_bench.pop);
		if (obj_bench.type_mode == TYPE_MODE_RAND)
			free(obj_bench.random_types);
	}
	free(obj_bench.alloc_sizes);
	return 0;
}

/* obj_list_clo -- array defining common command line arguments. */
static struct benchmark_clo obj_list_clo[6];

static struct benchmark_info obj_insert;
static struct benchmark_info obj_remove;
static struct benchmark_info obj_insert_new;
static struct benchmark_info obj_remove_free;
static struct benchmark_info obj_move;

CONSTRUCTOR(pmem_atomic_list_constructor)
void
pmem_atomic_list_constructor(void)
{
	obj_list_clo[0].opt_short = 'T';
	obj_list_clo[0].opt_long = "type-number";
	obj_list_clo[0].descr = "Type number mode - one, per-thread, "
				"rand";
	obj_list_clo[0].def = "one";
	obj_list_clo[0].off = clo_field_offset(struct obj_list_args, type_num);
	obj_list_clo[0].type = CLO_TYPE_STR;

	obj_list_clo[1].opt_short = 'P';
	obj_list_clo[1].opt_long = "position";
	obj_list_clo[1].descr = "Place where operation will be "
				"performed - head, tail, rand, middle";
	obj_list_clo[1].def = "middle";
	obj_list_clo[1].off = clo_field_offset(struct obj_list_args, position);
	obj_list_clo[1].type = CLO_TYPE_STR;

	obj_list_clo[2].opt_short = 'l';
	obj_list_clo[2].opt_long = "list-len";
	obj_list_clo[2].type = CLO_TYPE_UINT;
	obj_list_clo[2].descr = "Initial list len";
	obj_list_clo[2].off = clo_field_offset(struct obj_list_args, list_len);
	obj_list_clo[2].def = "1";
	obj_list_clo[2].type_uint.size =
		clo_field_size(struct obj_list_args, list_len);
	obj_list_clo[2].type_uint.base = CLO_INT_BASE_DEC | CLO_INT_BASE_HEX;
	obj_list_clo[2].type_uint.min = 1;
	obj_list_clo[2].type_uint.max = ULONG_MAX;

	obj_list_clo[3].opt_short = 'm';
	obj_list_clo[3].opt_long = "min-size";
	obj_list_clo[3].type = CLO_TYPE_UINT;
	obj_list_clo[3].descr = "Min allocation size";
	obj_list_clo[3].off = clo_field_offset(struct obj_list_args, min_size);
	obj_list_clo[3].def = "0";
	obj_list_clo[3].type_uint.size =
		clo_field_size(struct obj_list_args, min_size);
	obj_list_clo[3].type_uint.base = CLO_INT_BASE_DEC;
	obj_list_clo[3].type_uint.min = 0;
	obj_list_clo[3].type_uint.max = UINT_MAX;

	obj_list_clo[4].opt_short = 's';
	obj_list_clo[4].type_uint.max = INT_MAX;
	obj_list_clo[4].opt_long = "seed";
	obj_list_clo[4].type = CLO_TYPE_UINT;
	obj_list_clo[4].descr = "Seed value";
	obj_list_clo[4].off = clo_field_offset(struct obj_list_args, seed);
	obj_list_clo[4].def = "0";
	obj_list_clo[4].type_uint.size =
		clo_field_size(struct obj_list_args, seed);
	obj_list_clo[4].type_uint.base = CLO_INT_BASE_DEC;
	obj_list_clo[4].type_uint.min = 0;

	/*
	 * nclos field in benchmark_info structures is decremented to make
	 * queue option available only for obj_isert, obj_remove
	 */
	obj_list_clo[5].opt_short = 'q';
	obj_list_clo[5].opt_long = "queue";
	obj_list_clo[5].descr = "Use circleq from queue.h instead "
				"pmemobj";
	obj_list_clo[5].type = CLO_TYPE_FLAG;
	obj_list_clo[5].off = clo_field_offset(struct obj_list_args, queue);

	obj_insert.name = "obj_insert";
	obj_insert.brief = "pmemobj_list_insert() benchmark";
	obj_insert.init = obj_init;
	obj_insert.exit = obj_exit;
	obj_insert.multithread = true;
	obj_insert.multiops = true;
	obj_insert.init_worker = obj_insert_init_worker;
	obj_insert.free_worker = free_worker_items;
	obj_insert.operation = insert_op;
	obj_insert.measure_time = true;
	obj_insert.clos = obj_list_clo;
	obj_insert.nclos = ARRAY_SIZE(obj_list_clo);
	obj_insert.opts_size = sizeof(struct obj_list_args);
	obj_insert.rm_file = true;
	obj_insert.allow_poolset = true;

	REGISTER_BENCHMARK(obj_insert);

	obj_remove.name = "obj_remove";
	obj_remove.brief = "pmemobj_list_remove() benchmark "
			   "without freeing element";
	obj_remove.init = obj_init;
	obj_remove.exit = obj_exit;
	obj_remove.multithread = true;
	obj_remove.multiops = true;
	obj_remove.init_worker = obj_remove_init_worker;
	obj_remove.free_worker = free_worker_items;
	obj_remove.operation = remove_op;
	obj_remove.measure_time = true;
	obj_remove.clos = obj_list_clo;
	obj_remove.nclos = ARRAY_SIZE(obj_list_clo);
	obj_remove.opts_size = sizeof(struct obj_list_args);
	obj_remove.rm_file = true;
	obj_remove.allow_poolset = true;

	REGISTER_BENCHMARK(obj_remove);

	obj_insert_new.name = "obj_insert_new";
	obj_insert_new.brief = "pmemobj_list_insert_new() benchmark";
	obj_insert_new.init = obj_init;
	obj_insert_new.exit = obj_exit;
	obj_insert_new.multithread = true;
	obj_insert_new.multiops = true;
	obj_insert_new.init_worker = obj_insert_new_init_worker;
	obj_insert_new.free_worker = free_worker_list;
	obj_insert_new.operation = obj_insert_new_op;
	obj_insert_new.measure_time = true;
	obj_insert_new.clos = obj_list_clo;
	obj_insert_new.nclos = ARRAY_SIZE(obj_list_clo) - 1;
	obj_insert_new.opts_size = sizeof(struct obj_list_args);
	obj_insert_new.rm_file = true;
	obj_insert_new.allow_poolset = true;
	REGISTER_BENCHMARK(obj_insert_new);

	obj_remove_free.name = "obj_remove_free";
	obj_remove_free.brief = "pmemobj_list_remove() benchmark "
				"with freeing element";
	obj_remove_free.init = obj_init;
	obj_remove_free.exit = obj_exit;
	obj_remove_free.multithread = true;
	obj_remove_free.multiops = true;
	obj_remove_free.init_worker = obj_remove_init_worker;
	obj_remove_free.free_worker = free_worker_list;
	obj_remove_free.operation = obj_remove_free_op;
	obj_remove_free.measure_time = true;
	obj_remove_free.clos = obj_list_clo;
	obj_remove_free.nclos = ARRAY_SIZE(obj_list_clo) - 1;
	obj_remove_free.opts_size = sizeof(struct obj_list_args);
	obj_remove_free.rm_file = true;
	obj_remove_free.allow_poolset = true;
	REGISTER_BENCHMARK(obj_remove_free);

	obj_move.name = "obj_move";
	obj_move.brief = "pmemobj_list_move() benchmark";
	obj_move.init = obj_init;
	obj_move.exit = obj_exit;
	obj_move.multithread = true;
	obj_move.multiops = true;
	obj_move.init_worker = obj_move_init_worker;
	obj_move.free_worker = obj_move_free_worker;
	obj_move.operation = obj_move_op;
	obj_move.measure_time = true;
	obj_move.clos = obj_list_clo;
	obj_move.nclos = ARRAY_SIZE(obj_list_clo) - 1;
	obj_move.opts_size = sizeof(struct obj_list_args);
	obj_move.rm_file = true;
	obj_move.allow_poolset = true;
	REGISTER_BENCHMARK(obj_move);
}
