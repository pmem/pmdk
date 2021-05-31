// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/*
 * arenas.c -- implementation of the arenas, heap's scalability mechanism
 *
 * Arena is the allocators unit of concurrency. By default, each thread
 * is assigned an arena in a round robin fashion. There are as many arenas
 * as there are CPU cores available. This ensures that allocations are
 * distributed among different arenas, reducing lock contention.
 */

#include "arenas.h"
#include "bucket.h"
#include "container_seglists.h"
#include "alloc_class.h"
#include "memblock.h"
#include "sys_util.h"
#include "vec.h"

#define MAX_DEFAULT_ARENAS (1 << 10) /* 1024 arenas */
enum pobj_arenas_assignment_type Default_arenas_assignment_type =
	POBJ_ARENAS_ASSIGNMENT_THREAD_KEY;

struct arenas_thread_assignment {
	enum pobj_arenas_assignment_type type;
	union {
		os_tls_key_t thread;
		struct arena *global;
	};
};

struct arenas {
	VEC(, struct arena *) vec;
	size_t nactive;

	/*
	 * When nesting with other locks, this one must be acquired first,
	 * prior to locking any buckets or memory blocks.
	 */
	os_mutex_t lock;

	/* stores a pointer to one of the arenas */
	struct arenas_thread_assignment assignment;
};

/*
 * Arenas store the collection of buckets for allocation classes.
 * Each thread is assigned an arena on its first allocator operation
 * if arena is set to auto.
 */
struct arena {
	/* one bucket per allocation class */
	struct bucket_locked *buckets[MAX_ALLOCATION_CLASSES];

	/*
	 * Decides whether the arena can be
	 * automatically assigned to a thread.
	 */
	int automatic;
	size_t nthreads;
	struct arenas *arenas;
};

/*
 * arenas_thread_detach -- detaches arena from the current thread
 *
 * Must be called with arenas lock taken.
 */
static void
arenas_thread_detach(struct arena *a)
{
	/*
	 * Even though this is under a lock, nactive variable can also be read
	 * concurrently from the recycler (without the arenas lock).
	 * That's why we are using an atomic operation.
	 */
	if ((--a->nthreads) == 0)
		util_fetch_and_sub64(&a->arenas->nactive, 1);
}

/*
 * arena_thread_destructor -- (internal) removes arena thread assignment
 */
static void
arena_thread_destructor(void *arg)
{
	struct arena *a = arg;
	util_mutex_lock(&a->arenas->lock);
	arenas_thread_detach(a);
	util_mutex_unlock(&a->arenas->lock);
}

/*
 * arenas_thread_assignment_init -- (internal) initializes thread assignment
 *	type for arenas.
 */
static int
arenas_thread_assignment_init(struct arenas_thread_assignment *assignment,
	enum pobj_arenas_assignment_type type)
{
	assignment->type = type;

	int ret = 0;

	switch (type) {
		case POBJ_ARENAS_ASSIGNMENT_THREAD_KEY:
			ret = os_tls_key_create(&assignment->thread,
				arena_thread_destructor);
			break;
		case POBJ_ARENAS_ASSIGNMENT_GLOBAL:
			assignment->global = NULL;
			break;
		default: {
			ASSERT(0); /* unreachable */
		}
	}

	return ret;
}

/*
 * arena_thread_assignment_fini -- (internal) destroys thread assignment
 *	type for arenas.
 */
static void
arenas_thread_assignment_fini(struct arenas_thread_assignment *assignment)
{
	switch (assignment->type) {
		case POBJ_ARENAS_ASSIGNMENT_THREAD_KEY:
			os_tls_key_delete(assignment->thread);
			break;
		case POBJ_ARENAS_ASSIGNMENT_GLOBAL:
			break;
		default: {
			ASSERT(0); /* unreachable */
		}
	}
}

/*
 * arenas_new - creates a new arenas instance
 */
struct arenas *
arenas_new(void)
{
	struct arenas *arenas = Malloc(sizeof(*arenas));
	if (arenas == NULL)
		goto error_arenas_malloc;

	util_mutex_init(&arenas->lock);
	VEC_INIT(&arenas->vec);
	arenas->nactive = 0;

	if (VEC_RESERVE(&arenas->vec, MAX_DEFAULT_ARENAS) == -1)
		goto error_vec_reserve;

	if (arenas_thread_assignment_init(&arenas->assignment,
		Default_arenas_assignment_type) != 0)
		goto error_thread_assignment_init;

	return arenas;

error_thread_assignment_init:
	VEC_DELETE(&arenas->vec);
error_vec_reserve:
	Free(arenas);
error_arenas_malloc:
	return NULL;
}

/*
 * arena_delete -- (internal) deletes an arena instance
 */
static void
arena_delete(struct arena *arena)
{
	for (int i = 0; i < MAX_ALLOCATION_CLASSES; ++i)
		if (arena->buckets[i] != NULL)
			bucket_locked_delete(arena->buckets[i]);
	Free(arena);
}

/*
 * arenas_delete -- (internal) deletes all arenas
 */
void
arenas_delete(struct arenas *arenas)
{
	arenas_thread_assignment_fini(&arenas->assignment);

	struct arena *arena;
	VEC_FOREACH(arena, &arenas->vec) {
		arena_delete(arena);
	}

	util_mutex_destroy(&arenas->lock);
	VEC_DELETE(&arenas->vec);

	Free(arenas);
}

/*
 * arena_new -- (internal) creates a new arena instance
 */
static struct arena *
arena_new(struct arenas *arenas, struct alloc_class_collection *aclasses,
	int automatic)
{
	struct arena *arena = Malloc(sizeof(struct arena));
	if (arena == NULL) {
		ERR("!heap: arena malloc error");
		return NULL;
	}
	arena->nthreads = 0;
	arena->automatic = automatic;
	arena->arenas = arenas;

	COMPILE_ERROR_ON(MAX_ALLOCATION_CLASSES > UINT8_MAX);
	for (uint8_t i = 0; i < MAX_ALLOCATION_CLASSES; ++i) {
		struct alloc_class *ac =
			alloc_class_by_id(aclasses, i);
		if (ac != NULL) {
			arena->buckets[i] =
				bucket_locked_new(container_new_seglists(),
					ac);
			if (arena->buckets[i] == NULL)
				goto error_bucket_create;
		} else {
			arena->buckets[i] = NULL;
		}
	}

	return arena;

error_bucket_create:
	arena_delete(arena);
	return NULL;
}

/*
 * arenas_create_all -- creates all arenas for the alloc class collection
 */
int
arenas_create_all(struct arenas *arenas,
	struct alloc_class_collection *aclasses,
	unsigned narenas)
{
	for (unsigned i = 0; i < narenas; ++i) {
		struct arena *arena = arena_new(arenas, aclasses, 1);
		if (VEC_PUSH_BACK(&arenas->vec, arena) != 0) {
			/* cleanup happens in arenas_delete */
			return -1;
		}
	}

	return 0;
}

/*
 * arenas_get_arena_by_id -- returns arena with the given id
 */
struct arena *
arenas_get_arena_by_id(struct arenas *arenas, unsigned id)
{
	return id == 0 ? NULL : VEC_ARR(&arenas->vec)[id - 1];
}

/*
 * arena_thread_attach -- assign arena to the current thread
 *
 * Must be called with arenas lock taken.
 */
static void
arenas_thread_attach(struct arenas *arenas, struct arena *a)
{
	struct arenas_thread_assignment *assignment = &arenas->assignment;
	ASSERTeq(assignment->type, POBJ_ARENAS_ASSIGNMENT_THREAD_KEY);

	struct arena *thread_arena = os_tls_get(assignment->thread);
	if (thread_arena)
		arenas_thread_detach(thread_arena);

	ASSERTne(a, NULL);

	/*
	 * Even though this is under a lock, nactive variable can also be read
	 * concurrently from the recycler (without the arenas lock).
	 * That's why we are using an atomic operation.
	 */
	if ((a->nthreads++) == 0)
		util_fetch_and_add64(&a->arenas->nactive, 1);

	os_tls_set(assignment->thread, a);
}

/*
 * arenas_global_assign -- (internal) assigns the first automatic arena
 *	as the heaps' global arena assignment.
 */
static struct arena *
arenas_global_assign(struct arenas *arenas)
{
	util_mutex_lock(&arenas->lock);

	ASSERTne(VEC_SIZE(&arenas->vec), 0);

	struct arena *a = NULL;
	VEC_FOREACH(a, &arenas->vec) {
		if (a->automatic)
			break;
	}

	LOG(4, "assigning %p arena to current thread", a);

	/* at least one automatic arena must exist */
	ASSERTne(a, NULL);
	arenas->assignment.global = a;

	util_mutex_unlock(&arenas->lock);

	return a;
}

/*
 * arenas_thread_assign -- (internal) assigns the least used arena
 *	to current thread
 *
 * To avoid complexities with regards to races in the search for the least
 * used arena, a lock is used, but the nthreads counter of the arena is still
 * bumped using atomic instruction because it can happen in parallel to a
 * destructor of a thread, which also touches that variable.
 */
static struct arena *
arenas_thread_assign(struct arenas *arenas)
{
	util_mutex_lock(&arenas->lock);

	struct arena *least_used = NULL;

	ASSERTne(VEC_SIZE(&arenas->vec), 0);

	struct arena *a;
	VEC_FOREACH(a, &arenas->vec) {
		if (!a->automatic)
			continue;
		if (least_used == NULL ||
			a->nthreads < least_used->nthreads)
			least_used = a;
	}

	LOG(4, "assigning %p arena to current thread", least_used);

	/* at least one automatic arena must exist */
	ASSERTne(least_used, NULL);
	arenas_thread_attach(arenas, least_used);

	util_mutex_unlock(&arenas->lock);

	return least_used;
}

/*
 * arenas_get_arena_by_assignment -- (internal) returns the arena assigned to
 *	the current thread
 */
struct arena *
arenas_get_arena_by_assignment(struct arenas *arenas)
{
	struct arenas_thread_assignment *assignment =
		&arenas->assignment;
	struct arena *arena = NULL;

	switch (assignment->type) {
		case POBJ_ARENAS_ASSIGNMENT_THREAD_KEY:
			if ((arena = os_tls_get(assignment->thread)) == NULL)
				arena = arenas_thread_assign(arenas);
			break;
		case POBJ_ARENAS_ASSIGNMENT_GLOBAL:
			if ((arena = assignment->global) == NULL)
				arena = arenas_global_assign(arenas);
			break;
		default: {
			ASSERT(0); /* unreachable */
		}
	}

	ASSERTne(arena, NULL);

	return arena;
}

/*
 * arenas_create_buckets_for_alloc_class -- creates buckets for the given alloc
 *	class in all arenas
 */
int
arenas_create_buckets_for_alloc_class(struct arenas *arenas,
	struct alloc_class *aclass)
{
	util_mutex_lock(&arenas->lock);
	struct arena *arena;
	VEC_FOREACH(arena, &arenas->vec) {
		if (arena->buckets[aclass->id] == NULL) {
			arena->buckets[aclass->id] = bucket_locked_new(
				container_new_seglists(), aclass);
			if (arena->buckets[aclass->id] == NULL)
				goto error_bucket_new;
		}
	}

	util_mutex_unlock(&arenas->lock);
	return 0;

error_bucket_new:
	VEC_FOREACH(arena, &arenas->vec) {
		if (arena->buckets[aclass->id] != NULL) {
			bucket_locked_delete(arena->buckets[aclass->id]);
			arena->buckets[aclass->id] = NULL;
		}
	}

	util_mutex_unlock(&arenas->lock);
	return -1;
}

/*
 * arenas_dynamic_count -- returns a pointer to the number of active arenas
 */
size_t *
arenas_dynamic_count(struct arenas *arenas)
{
	return &arenas->nactive;
}

/*
 * arenas_foreach_arena -- iterates over all arenas
 */
int
arenas_foreach_arena(struct arenas *arenas,
	arena_callback cb, void *arg)
{
	util_mutex_lock(&arenas->lock);

	struct arena *arena;
	VEC_FOREACH(arena, &arenas->vec) {
		int ret;
		if ((ret = cb(arena, arg)) != 0)
			return ret;
	}

	util_mutex_unlock(&arenas->lock);

	return 0;
}

/*
 * arena_foreach_bucket -- iterates over all buckets in the arena
 */
int
arena_foreach_bucket(struct arena *arena, bucket_callback cb, void *arg)
{
	for (int i = 0; i < MAX_ALLOCATION_CLASSES; ++i) {
		struct bucket_locked *locked = arena->buckets[i];
		if (locked == NULL)
			continue;

		int ret;
		if ((ret = cb(locked, arg)) != 0)
			return ret;
	}
	return 0;
}

/*
 * arena_get_bucket_by_id -- returns a bucket with the given id
 */
struct bucket_locked *
arena_get_bucket_by_id(struct arena *arena, unsigned id)
{
	return arena->buckets[id];
}

/*
 * arenas_force_thread_assignment -- assigns a specific arena for the current
 *	thread
 */
int
arenas_force_thread_assignment(struct arenas *arenas, unsigned id)
{
	util_mutex_lock(&arenas->lock);

	struct arena *arena;
	if ((arena = arenas_get_arena_by_id(arenas, id)) == NULL) {
		util_mutex_unlock(&arenas->lock);
		return -1;
	}

	arenas_thread_attach(arenas, arena);

	util_mutex_unlock(&arenas->lock);

	return 0;
}

/*
 * arenas_total -- returns the total number of arenas
 */
unsigned
arenas_total(struct arenas *arenas)
{
	size_t total;

	util_mutex_lock(&arenas->lock);

	total = VEC_SIZE(&arenas->vec);

	util_mutex_unlock(&arenas->lock);

	return (unsigned)total;
}

/*
 * arenas_max -- returns the maximum allowed number of arenas
 */
unsigned
arenas_max(struct arenas *arenas)
{
	size_t max;

	util_mutex_lock(&arenas->lock);

	max = VEC_CAPACITY(&arenas->vec);

	util_mutex_unlock(&arenas->lock);

	return (unsigned)max;
}

/*
 * arenas_increase_max -- increases the maximum numbers of allowed arenas
 */
int
arenas_increase_max(struct arenas *arenas, unsigned new_max)
{
	int ret = -1;

	util_mutex_lock(&arenas->lock);
	unsigned capacity = (unsigned)VEC_CAPACITY(&arenas->vec);
	if (new_max < capacity) {
		LOG(2, "cannot decrease max number of arenas");
	} else if (new_max == capacity) {
		ret = 0;
	} else {
		ret = VEC_RESERVE(&arenas->vec, new_max);
	}

	util_mutex_unlock(&arenas->lock);
	return ret;
}

/*
 * arenas_total_automatic -- total number of auto-assigning arenas
 */
unsigned
arenas_total_automatic(struct arenas *arenas)
{
	struct arena *arena;
	unsigned total = 0;

	util_mutex_lock(&arenas->lock);

	VEC_FOREACH(arena, &arenas->vec) {
		if (arena->automatic)
			total++;
	}

	util_mutex_unlock(&arenas->lock);

	return total;
}

/*
 * arena_get_id -- returns id of the given arena
 */
unsigned
arena_get_id(struct arena *arena)
{
	unsigned arena_id = 1;

	struct arenas *arenas = arena->arenas;

	util_mutex_lock(&arenas->lock);

	struct arena *arenav;
	VEC_FOREACH(arenav, &arenas->vec) {
		if (arenav == arena) {
			break;
		}
		arena_id++;
	}

	util_mutex_unlock(&arenas->lock);
	return arena_id;
}

/*
 * arenas_create_arena -- creates and initializes a new arena
 */
int
arenas_create_arena(struct arenas *arenas,
	struct alloc_class_collection *aclasses)
{
	struct arena *arena = arena_new(arenas, aclasses, 0);
	if (arena == NULL)
		return -1;

	util_mutex_lock(&arenas->lock);

	if (VEC_PUSH_BACK(&arenas->vec, arena))
		goto err_push_back;

	int ret = (int)VEC_SIZE(&arenas->vec);
	util_mutex_unlock(&arenas->lock);

	return ret;

err_push_back:
	util_mutex_unlock(&arenas->lock);
	arena_delete(arena);
	return -1;
}

/*
 * arena_is_automatic -- returns arena disposition
 */
int
arena_is_automatic(struct arena *arena)
{
	return arena->automatic;
}

/*
 * arena_set_automatic -- configures arena disposition
 */
int
arena_set_automatic(struct arena *arena, int automatic)
{
	unsigned total_automatic = arenas_total_automatic(arena->arenas);
	if (arena->automatic && automatic == 0 && total_automatic <= 1) {
		ERR("at least one automatic arena must exist");
		return -1;
	}

	arena->automatic = automatic;

	return 0;
}

/*
 * arena_estimated_size -- returns the estimated size utilized by the given
 *	arena
 */
size_t
arena_estimated_size(struct arena *arena)
{
	/* calculate number of reservation for arena using buckets */
	unsigned size = 0;
	for (int i = 0; i < MAX_ALLOCATION_CLASSES; ++i) {
		struct bucket_locked *locked = arena->buckets[i];

		if (locked != NULL) {
			struct bucket *b = bucket_acquire(locked);
			struct memory_block_reserved *active =
				bucket_active_block(b);

			size += active ? active->m.size_idx : 0;
			bucket_release(b);
		}
	}

	return size * CHUNKSIZE;
}
