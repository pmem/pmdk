/*
 * Copyright 2015-2016, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *      * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *      * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *      * Neither the name of the copyright holder nor the names of its
 *        contributors may be used to endorse or promote products derived
 *        from this software without specific prior written permission.
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
 * pmemobj_atomic_lists.c -- benchmark for pmemobj atomic list API
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/queue.h>

#include "libpmemobj.h"
#include "benchmark.h"

#define FACTOR 8
#define LAYOUT_NAME "benchmark"

struct obj_bench;
struct obj_worker;
struct element;

TOID_DECLARE(struct item, 0);
TOID_DECLARE(struct list, 1);

typedef unsigned (*fn_type_num) (unsigned worker_idx,
							unsigned op_idx);

typedef struct element (*fn_position) (struct obj_worker *obj_worker,
							unsigned op_idx);

typedef int (*fn_init) (struct worker_info *worker, size_t n_elm,
							size_t list_len);
/*
 * args -- stores command line parsed arguments.
 */
struct obj_list_args {
	char *type_num;		/* type_number mode - one, per-thread, rand */
	char *position;		/* position - head, tail, middle, rand */
	unsigned list_len;	/* initial list length */
	bool queue;		/* use circle queue from <sys/queue.h> */
	bool range;		/* use random allocation size */
	unsigned min_size;		/* minimum random allocation size */
	unsigned seed;	/* seed value */
};

/*
 * obj_bench -- stores variables used in benchmark, passed within functions.
 */
struct obj_bench {

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
	bool increment;
	size_t *alloc_sizes;	/* array to store random sizes of each object */
	size_t max_len;		/* maximum list length */
	size_t min_len;		/* initial list length */
	int type_mode;		/* type_number mode */
	int position_mode;	/* list destination mode */

	/*
	 * fn_type_num gets proper function assigned, depending on the
	 * value of the type_mode argument, which returns proper type number for
	 * each persistent object. Possible functions are:
	 *	- type_mode_one,
	 *	- type_mode_per_thread,
	 *	- type_mode_rand.
	 */
	fn_type_num fn_type_num;

	/*
	 * fn_position gets proper function assigned, depending  on the value
	 * of the position argument, which returns handle to proper element on
	 * the list. Possible functions are:
	 *	- position_head,
	 *	- position_tail,
	 *	- position_middle,
	 *	- position_rand.
	 */
	fn_position fn_position;

	/*
	 * fn_init gets proper function assigned, depending on the file_io
	 * flag, which allocates objects and initializes proper list. Possible
	 * functions are:
	 *	- obj_init_list,
	 *	- queue_init_list.
	 */
	fn_init fn_init;
} obj_bench;

/*
 * item -- structure used to connect elements in lists.
 */
struct item {
	POBJ_LIST_ENTRY(struct item) field;
	CIRCLEQ_ENTRY(item) fieldq;
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
	CIRCLEQ_HEAD(qlist, item) headq;
	TOID(struct item) *oids;	/* persistent pmemobj list elements */
	struct item **items;		/* volatile elements */
	size_t n_elm;			/* number of elements in array */
	size_t *positions;		/* place on the list if rand mode */
	struct element elm;		/* pointer to current element */
	/*
	 * list_move is a pointer to structure storing variables used by
	 * second list (used only for obj_move benchmark).
	 */
	struct obj_worker *list_move;
};

/* obj_list_clo -- array defining common command line arguments. */
static struct benchmark_clo obj_list_clo[] = {
	{
		.opt_short	= 'T',
		.opt_long	= "type-number",
		.descr		= "Type number mode - one, per-thread, rand",
		.def		= "one",
		.off		= clo_field_offset(struct obj_list_args,
								type_num),
		.type		= CLO_TYPE_STR,
	},
	{
		.opt_short	= 'P',
		.opt_long	= "position",
		.descr		= "Place where operation will be performed -"
					" head, tail, rand, middle",
		.def		= "middle",
		.off		= clo_field_offset(struct obj_list_args,
								position),
		.type		= CLO_TYPE_STR,
	},
	{
		.opt_short	= 'l',
		.opt_long	= "list-len",
		.type		= CLO_TYPE_UINT,
		.descr		= "Initial list len",
		.off		= clo_field_offset(struct obj_list_args,
						list_len),
		.def		= "1",
		.type_uint	= {
			.size	= clo_field_size(struct obj_list_args,
						list_len),
			.base	= CLO_INT_BASE_DEC|CLO_INT_BASE_HEX,
			.min	= 1,
			.max	= ULONG_MAX,
		},
	},
	{
		.opt_short	= 'm',
		.opt_long	= "min-size",
		.type		= CLO_TYPE_UINT,
		.descr		= "Min allocation size",
		.off		= clo_field_offset(struct obj_list_args,
							min_size),
		.def		= "0",
		.type_uint	= {
			.size	= clo_field_size(struct obj_list_args,
								min_size),
			.base	= CLO_INT_BASE_DEC,
			.min	= 0,
			.max	= UINT_MAX,
		},
	},
	{
		.opt_short	= 's',
		.opt_long	= "seed",
		.type		= CLO_TYPE_UINT,
		.descr		= "Seed value",
		.off		= clo_field_offset(struct obj_list_args, seed),
		.def		= "0",
		.type_uint	= {
			.size	= clo_field_size(struct obj_list_args, seed),
			.base	= CLO_INT_BASE_DEC,
			.min	= 0,
			.max	= INT_MAX,
		},
	},
	/*
	 * nclos field in benchmark_info structures is decremented to make
	 * queue option available only for obj_isert, obj_remove
	 */
	{
		.opt_short	= 'q',
		.opt_long	= "queue",
		.descr		= "Use circleq from queue.h instead pmemobj",
		.type		= CLO_TYPE_FLAG,
		.off		= clo_field_offset(struct obj_list_args, queue),
	},
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
	TYPE_MODE_ONE,		/* one type number for all of objects */

	/* one type number for objects allocated by the same thread */
	TYPE_MODE_PER_THREAD,
	TYPE_MODE_RAND,		/* random type number for each object */
	TYPE_MODE_UNKNOWN,
};

/*
 * position_head -- returns head of the persistent list or volatile queue.
 */
static struct element
position_head(struct obj_worker *obj_worker, unsigned op_idx)
{
	struct element head = {0};
	head.before = true;
	if (!obj_bench.args->queue)
		head.itemp = POBJ_LIST_FIRST(&obj_worker->head);
	else
		head.itemq = CIRCLEQ_FIRST(&obj_worker->headq);
	return head;
}

/*
 * position_tail -- returns tail of the persistent list or volatile queue.
 */
static struct element
position_tail(struct obj_worker *obj_worker, unsigned op_idx)
{
	struct element tail = {0};
	tail.before = false;
	if (!obj_bench.args->queue)
		tail.itemp = POBJ_LIST_LAST(&obj_worker->head, field);
	else
		tail.itemq = CIRCLEQ_LAST(&obj_worker->headq);
	return tail;
}

/*
 * position_middle -- returns second or first element from the persistent list
 * or volatile queue.
 */
static struct element
position_middle(struct obj_worker *obj_worker, unsigned op_idx)
{
	struct element elm = position_head(obj_worker, op_idx);
	elm.before = true;
	if (!obj_bench.args->queue)
		elm.itemp = POBJ_LIST_NEXT(elm.itemp, field);
	else
		elm.itemq = CIRCLEQ_NEXT(elm.itemq, fieldq);
	return elm;
}

static struct item *
queue_get_item(struct obj_worker *obj_worker, unsigned idx)
{
	struct item *item;
	CIRCLEQ_FOREACH(item, &obj_worker->headq, fieldq) {
		if (idx == 0)
			return item;
		idx--;
	}
	return NULL;
}

static TOID(struct item)
obj_get_item(struct obj_worker *obj_worker, unsigned idx)
{
	TOID(struct item) oid;
	POBJ_LIST_FOREACH(oid, &obj_worker->head, field) {
		if (idx == 0)
			return oid;
		idx--;
	}
	return TOID_NULL(struct item);
}

/*
 * position_rand -- returns first, second or last element from the persistent
 * list or volatile queue based on r_positions array.
 */
static struct element
position_rand(struct obj_worker *obj_worker, unsigned op_idx)
{
	struct element elm = {0};
	elm.before = true;
	if (!obj_bench.args->queue)
		elm.itemp = obj_get_item(obj_worker,
						obj_worker->positions[op_idx]);
	else
		elm.itemq = queue_get_item(obj_worker,
						obj_worker->positions[op_idx]);
	return elm;

}

/*
 * type_mode_one -- always returns 0, as in the mode TYPE_MODE_ONE
 * all of the persistent objects have the same type_number value.
 */
static unsigned
type_mode_one(unsigned worker_idx, unsigned op_idx)
{
	return 0;
}

/*
 * type_mode_per_thread -- always returns the index of the worker,
 * as in the TYPE_MODE_PER_THREAD the value of the persistent object
 * type_number is specific to the thread.
 */
static unsigned
type_mode_per_thread(unsigned worker_idx, unsigned op_idx)
{
	return worker_idx;
}

/*
 * type_mode_rand -- returns the value from the random_types array assigned
 * for the specific operation in a specific thread.
 */
static unsigned
type_mode_rand(unsigned worker_idx, unsigned op_idx)
{
	return obj_bench.random_types[op_idx];
}

char *type_num_names[] = {"one", "per-thread", "rand"};
char *position_names[] = {"head", "tail", "middle", "rand"};
static fn_type_num type_num_modes[] = {type_mode_one, type_mode_per_thread,
								type_mode_rand};
static fn_position positions[] = {position_head, position_tail, position_middle,
								position_rand};
/*
 * parse_args -- parse command line string argument
 */
static int
parse_args(char *arg, int max, char **names)
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
	struct obj_worker *obj_worker = worker->priv;
	obj_worker->oids = calloc(n_oids, sizeof(TOID(struct item)));
	if (obj_worker->oids == NULL) {
		perror("calloc");
		return -1;
	}
	for (i = 0; i < n_oids; i++) {
		size_t type_num = obj_bench.fn_type_num(worker->index, i);
		size_t size = obj_bench.alloc_sizes[i];
		PMEMoid *tmp = (PMEMoid *)&obj_worker->oids[i];
		if (pmemobj_alloc(obj_bench.pop, tmp, size, type_num,
							NULL, NULL) != 0)
			goto err_oids;
	}
	for (i = 0; i < list_len; i++)
		POBJ_LIST_INSERT_TAIL(obj_bench.pop, &obj_worker->head,
						obj_worker->oids[i], field);
	return 0;
err_oids:
	for (int j = i - 1; j >= 0; j--)
		POBJ_FREE(&obj_worker->oids[j]);
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
	struct obj_worker *obj_worker = worker->priv;
	CIRCLEQ_INIT(&obj_worker->headq);
	obj_worker->items = malloc(n_items * sizeof(struct item *));
	if (obj_worker->items == NULL) {
		perror("malloc");
		return -1;
	}

	for (i = 0; i < n_items; i++) {
		size_t size = obj_bench.alloc_sizes[i];
		obj_worker->items[i] = malloc(size);
		if (obj_worker->items[i] == NULL) {
			perror("malloc");
			goto err;
		}
	}

	for (i = 0; i < list_len; i++)
		CIRCLEQ_INSERT_TAIL(&obj_worker->headq, obj_worker->items[i],
								fieldq);

	return 0;
err:
	for (int j = i - 1; j >= 0; j--)
		free(obj_worker->items[j]);
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
	while (!CIRCLEQ_EMPTY(&obj_worker->headq)) {
		struct item *tmp = CIRCLEQ_LAST(&obj_worker->headq);
		CIRCLEQ_REMOVE(&obj_worker->headq, tmp, fieldq);
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
		POBJ_LIST_REMOVE_FREE(obj_bench.pop, &obj_worker->head,
				tmp, field);
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
static size_t *
random_positions()
{
	size_t *positions  = calloc(obj_bench.max_len, sizeof(size_t));
	if (positions == NULL) {
		perror("calloc");
		return NULL;
	}

	if (obj_bench.args->seed != 0)
		srand(obj_bench.args->seed);
	size_t list_len = obj_bench.increment ? obj_bench.min_len :
							obj_bench.max_len;
	for (size_t i = 0; i < obj_bench.max_len; i++) {
		positions[i] = RRAND(list_len, 0);
		list_len += obj_bench.increment ? 1 : -1;
	}
	return positions;
}

/*
 * rand_values -- allocates array and if range mode calculates random
 * values as allocation sizes for each object otherwise populates whole array
 * with max value. Used only when range flag set.
 */
static size_t *
random_values(size_t min, size_t max, unsigned n_ops, size_t min_range)
{
	size_t *randoms = calloc(n_ops, sizeof(size_t));
	if (randoms == NULL) {
		perror("calloc");
		return NULL;
	}
	for (size_t i = 0; i < n_ops; i++)
		randoms[i] = max;
	if (min > min_range) {
		if (min > max) {
			fprintf(stderr, "Invalid size\n");
			return NULL;
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
	struct obj_worker *obj_worker = info->worker->priv;
	CIRCLEQ_INSERT_AFTER(&obj_worker->headq, obj_worker->elm.itemq,
				obj_worker->items[info->index
				+ obj_bench.min_len], fieldq);
	return 0;
}

/*
 * obj_insert_op -- main operations of the obj_insert benchmark when queue flag
 * set to false.
 */
static int
obj_insert_op(struct operation_info *info)
{
	struct obj_worker *obj_worker = info->worker->priv;
	POBJ_LIST_INSERT_AFTER(obj_bench.pop, &obj_worker->head,
			obj_worker->elm.itemp, obj_worker->oids[info->index +
			obj_bench.min_len], field);
	return 0;
}

/*
 * queue_remove_op -- main operations of the obj_remove benchmark when queue
 * flag set to true.
 */
static int
queue_remove_op(struct operation_info *info)
{
	struct obj_worker *obj_worker = info->worker->priv;
	CIRCLEQ_REMOVE(&obj_worker->headq, obj_worker->elm.itemq, fieldq);
	return 0;
}

/*
 * obj_remove_op -- main operations of the obj_remove benchmark when queue flag
 * set to false.
 */
static int
obj_remove_op(struct operation_info *info)
{
	struct obj_worker *obj_worker = info->worker->priv;
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
	return obj_bench.args->queue ? queue_insert_op(info)
					: obj_insert_op(info);
}

/*
 * obj_insert_new_op -- main operations of the obj_insert_new benchmark.
 */
static int
obj_insert_new_op(struct benchmark *bench, struct operation_info *info)
{
	struct obj_worker *obj_worker = info->worker->priv;
	PMEMoid tmp;
	size_t size = obj_bench.alloc_sizes[info->index];
	unsigned type_num = obj_bench.fn_type_num(info->worker->index,
								info->index);
	tmp = pmemobj_list_insert_new(obj_bench.pop,
			offsetof(struct item, field), &obj_worker->head,
			obj_worker->elm.itemp.oid, obj_worker->elm.before,
			size, type_num, NULL, NULL);

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
	return obj_bench.args->queue ? queue_remove_op(info)
					: obj_remove_op(info);
}

/*
 * obj_remove_free_op -- main operation of the obj_remove_free benchmark.
 */
static int
obj_remove_free_op(struct benchmark *bench, struct operation_info *info)
{
	struct obj_worker *obj_worker = info->worker->priv;
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
	struct obj_worker *obj_worker = info->worker->priv;
	POBJ_LIST_MOVE_ELEMENT_BEFORE(obj_bench.pop, &obj_worker->head,
				&obj_worker->list_move->head,
				obj_worker->list_move->elm.itemp,
				obj_worker->elm.itemp, field, field);
	return 0;
}

/*
 * get_item -- common part of initial operation of the all benchmarks It gets
 * pointer to element on the list where object will
 * be inserted/removed/moved to/from.
 */
static int
get_item(struct benchmark *bench, struct operation_info *info)
{
	struct obj_worker *obj_worker = info->worker->priv;
	obj_worker->elm = obj_bench.fn_position(obj_worker, info->index);
	return 0;
}

/*
 * get_move_item -- special part of initial operation of the obj_move
 * benchmarks It gets pointer to element on the list where object will be
 * inserted/removed/moved to/from.
 */
static int
get_move_item(struct benchmark *bench, struct operation_info *info)
{
	struct obj_worker *obj_worker = info->worker->priv;
	obj_worker->list_move->elm =
		obj_bench.fn_position(obj_worker->list_move, info->index);
	return get_item(bench, info);
}

static void
free_worker(struct obj_worker *obj_worker)
{
	if (obj_bench.position_mode == POSITION_MODE_RAND)
		free(obj_worker->positions);
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
	struct obj_worker *obj_worker = worker->priv;
	obj_bench.args->queue ?	queue_free_worker_list(obj_worker) :
				obj_free_worker_list(obj_worker);
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
	struct obj_worker *obj_worker = worker->priv;
	struct obj_list_args *obj_args = args->opts;
	obj_args->queue ? queue_free_worker_items(obj_worker) :
					obj_free_worker_items(obj_worker);
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
	struct obj_worker *obj_worker = worker->priv;
	while (!POBJ_LIST_EMPTY(&obj_worker->list_move->head))
		POBJ_LIST_REMOVE_FREE(obj_bench.pop,
				&obj_worker->list_move->head,
				POBJ_LIST_LAST(&obj_worker->list_move->head,
				field), field);

	if (obj_bench.position_mode == POSITION_MODE_RAND)
		free(obj_worker->list_move->positions);
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
	struct obj_worker *obj_worker = calloc(1, sizeof(struct obj_worker));
	if (obj_worker == NULL) {
		perror("calloc");
		return -1;
	}

	worker->priv = obj_worker;
	obj_worker->n_elm = obj_bench.max_len;
	obj_worker->list_move = NULL;
	if (obj_bench.position_mode == POSITION_MODE_RAND) {
		obj_worker->positions = random_positions();
		if (obj_worker->positions == NULL)
			goto err;
	}
	if (obj_bench.fn_init(worker, n_elm, list_len) != 0)
		goto err_positions;

	return 0;
err_positions:
	free(obj_worker->positions);
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
	obj_bench.increment = true;
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
	obj_bench.increment = true;
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
	obj_bench.increment = false;
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
	obj_bench.increment = false;
	if (obj_init_worker(worker, obj_bench.max_len,
						obj_bench.max_len) != 0)
		return -1;

	struct obj_worker *obj_worker = worker->priv;
	obj_worker->list_move = calloc(1, sizeof(struct obj_worker));
	if (obj_worker->list_move == NULL) {
		perror("calloc");
		goto free;
	}
	size_t i;
	if (obj_bench.position_mode == POSITION_MODE_RAND) {
		obj_bench.increment = true;
		obj_worker->list_move->positions = random_positions();
		if (obj_worker->list_move->positions == NULL)
			goto free_list_move;
	}
	for (i = 0; i < obj_bench.min_len; i++) {
		size_t size = obj_bench.alloc_sizes[i];
		POBJ_LIST_INSERT_NEW_TAIL(obj_bench.pop,
				&obj_worker->list_move->head, field, size,
				NULL, NULL);
		if (TOID_IS_NULL(POBJ_LIST_LAST(&obj_worker->list_move->head,
								field))) {
			perror("pmemobj_list_insert_new");
			goto free_all;
		}
	}
	return 0;
free_all:
	for (int j = i - 1; j >= 0; j--) {
		POBJ_LIST_REMOVE_FREE(obj_bench.pop,
				&obj_worker->list_move->head,
				POBJ_LIST_LAST(&obj_worker->list_move->head,
				field), field);
	}
	free(obj_worker->list_move->positions);
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
	assert(bench != NULL);
	assert(args != NULL);
	assert(args->opts != NULL);

	obj_bench.args = args->opts;
	obj_bench.min_len = obj_bench.args->list_len + 1;
	obj_bench.max_len = args->n_ops_per_thread + obj_bench.min_len;

	obj_bench.fn_init = obj_bench.args->queue ? queue_init_list :
								obj_init_list;
	/* Decide if use random or state allocation sizes */
	size_t obj_size = args->dsize < sizeof(struct item) ?
				sizeof(struct item) : args->dsize;
	size_t min_size = obj_bench.args->min_size < sizeof(struct item) ?
				sizeof(struct item) : obj_bench.args->min_size;
	obj_bench.alloc_sizes = random_values(min_size, obj_size,
				obj_bench.max_len, sizeof(struct item));
	if (obj_bench.alloc_sizes == NULL)
		goto free_random_types;

	/* Decide where operations will be performed */
	obj_bench.position_mode = parse_args(obj_bench.args->position,
					POSITION_MODE_UNKNOWN, position_names);
	if (obj_bench.position_mode == POSITION_MODE_UNKNOWN)
			goto free_all;

	obj_bench.fn_position = positions[obj_bench.position_mode];
	if (!obj_bench.args->queue) {
		/* Decide what type number will be used */
		obj_bench.type_mode = parse_args(obj_bench.args->type_num,
					TYPE_MODE_UNKNOWN, type_num_names);
		if (obj_bench.type_mode == TYPE_MODE_UNKNOWN)
			return -1;

		obj_bench.fn_type_num = type_num_modes[obj_bench.type_mode];
		if (obj_bench.type_mode == TYPE_MODE_RAND) {
			obj_bench.random_types = random_values(1,
				UINT32_MAX, obj_bench.max_len, 0);
			if (obj_bench.random_types == NULL)
				return -1;
		}
		/*
		 * Multiplication by FACTOR prevents from out of memory error
		 * as the actual size of the allocated persistent objects
		 * is always larger than requested.
		 */
		size_t psize = (args->n_ops_per_thread + obj_bench.min_len + 1)
					* obj_size *
					args->n_threads * FACTOR;
		if (args->is_poolset) {
			if (args->fsize < psize) {
				fprintf(stderr, "insufficient size "
						"of poolset\n");
				goto free_all;
			}

			psize = 0;
		} else {
			if (psize < PMEMOBJ_MIN_POOL)
				psize = PMEMOBJ_MIN_POOL;
		}

		/* Create pmemobj pool. */
		if ((obj_bench.pop = pmemobj_create(args->fname, LAYOUT_NAME,
						psize, args->fmode)) == NULL) {
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

static struct benchmark_info obj_insert = {
	.name		= "obj_insert",
	.brief		= "pmemobj_list_insert() benchmark",
	.init		= obj_init,
	.exit		= obj_exit,
	.multithread	= true,
	.multiops	= true,
	.init_worker	= obj_insert_init_worker,
	.free_worker	= free_worker_items,
	.op_init	= get_item,
	.operation	= insert_op,
	.measure_time	= true,
	.clos		= obj_list_clo,
	.nclos		= ARRAY_SIZE(obj_list_clo),
	.opts_size	= sizeof(struct obj_list_args),
	.rm_file	= true,
	.allow_poolset	= true,
};
REGISTER_BENCHMARK(obj_insert);

static struct benchmark_info obj_remove = {
	.name		= "obj_remove",
	.brief		= "pmemobj_list_remove() benchmark without freeing"
								"element",
	.init		= obj_init,
	.exit		= obj_exit,
	.multithread	= true,
	.multiops	= true,
	.init_worker	= obj_remove_init_worker,
	.free_worker	= free_worker_items,
	.op_init	= get_item,
	.operation	= remove_op,
	.measure_time	= true,
	.clos		= obj_list_clo,
	.nclos		= ARRAY_SIZE(obj_list_clo),
	.opts_size	= sizeof(struct obj_list_args),
	.rm_file	= true,
	.allow_poolset	= true,
};
REGISTER_BENCHMARK(obj_remove);

static struct benchmark_info obj_insert_new = {
	.name		= "obj_insert_new",
	.brief		= "pmemobj_list_insert_new() benchmark",
	.init		= obj_init,
	.exit		= obj_exit,
	.multithread	= true,
	.multiops	= true,
	.init_worker	= obj_insert_new_init_worker,
	.free_worker	= free_worker_list,
	.op_init	= get_item,
	.operation	= obj_insert_new_op,
	.measure_time	= true,
	.clos		= obj_list_clo,
	.nclos		= ARRAY_SIZE(obj_list_clo) - 1,
	.opts_size	= sizeof(struct obj_list_args),
	.rm_file	= true,
	.allow_poolset	= true,
};
REGISTER_BENCHMARK(obj_insert_new);

static struct benchmark_info obj_remove_free = {
	.name		= "obj_remove_free",
	.brief		= "pmemobj_list_remove() benchmark with freeing"
								"element",
	.init		= obj_init,
	.exit		= obj_exit,
	.multithread	= true,
	.multiops	= true,
	.init_worker	= obj_remove_init_worker,
	.free_worker	= free_worker_list,
	.op_init	= get_item,
	.operation	= obj_remove_free_op,
	.measure_time	= true,
	.clos		= obj_list_clo,
	.nclos		= ARRAY_SIZE(obj_list_clo) - 1,
	.opts_size	= sizeof(struct obj_list_args),
	.rm_file	= true,
	.allow_poolset	= true,
};
REGISTER_BENCHMARK(obj_remove_free);

static struct benchmark_info obj_move = {
	.name		= "obj_move",
	.brief		= "pmemobj_list_move() benchmark",
	.init		= obj_init,
	.exit		= obj_exit,
	.multithread	= true,
	.multiops	= true,
	.init_worker	= obj_move_init_worker,
	.free_worker	= obj_move_free_worker,
	.op_init	= get_move_item,
	.operation	= obj_move_op,
	.measure_time	= true,
	.clos		= obj_list_clo,
	.nclos		= ARRAY_SIZE(obj_list_clo) - 1,
	.opts_size	= sizeof(struct obj_list_args),
	.rm_file	= true,
	.allow_poolset	= true,
};
REGISTER_BENCHMARK(obj_move);
