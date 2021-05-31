/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2021, Intel Corporation */

/*
 * arenas.h -- internal definitions for arenas
 */

#include <stddef.h>
#include <stdint.h>

#include "bucket.h"
#include "alloc_class.h"
#include "memblock.h"

extern enum pobj_arenas_assignment_type Default_arenas_assignment_type;
#define ARENA_DEFAULT_ASSIGNMENT (0)

struct arenas;
struct arena;

struct arenas *arenas_new(void);
void arenas_delete(struct arenas *arenas);

int arenas_create_all(struct arenas *arenas,
	struct alloc_class_collection *aclasses,
	unsigned narenas);

struct arena *arenas_get_arena_by_assignment(struct arenas *arenas);
struct arena *arenas_get_arena_by_id(struct arenas *arenas, unsigned id);

typedef int arena_callback(struct arena *, void *);
int arenas_foreach_arena(struct arenas *arenas,
	arena_callback callback, void *arg);

size_t *arenas_dynamic_count(struct arenas *arenas);

int arenas_create_buckets_for_alloc_class(struct arenas *,
	struct alloc_class *);

typedef int bucket_callback(struct bucket_locked *, void *);
int arena_foreach_bucket(struct arena *arena, bucket_callback, void *arg);

struct bucket_locked *arena_get_bucket_by_id(struct arena *arena, unsigned id);

int arenas_force_thread_assignment(struct arenas *arenas, unsigned arena_id);

unsigned arenas_total(struct arenas *arenas);

unsigned arenas_max(struct arenas *arenas);

int arenas_increase_max(struct arenas *arenas, unsigned new_max);

unsigned arenas_total_automatic(struct arenas *arenas);

unsigned arena_get_id(struct arena *arena);

int arenas_create_arena(struct arenas *arenas,
	struct alloc_class_collection *aclasses);

size_t arena_estimated_size(struct arena *arena);

int arena_is_automatic(struct arena *arena);
int arena_set_automatic(struct arena *arena, int automatic);
