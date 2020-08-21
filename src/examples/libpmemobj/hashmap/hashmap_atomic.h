/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2015-2020, Intel Corporation */
#ifndef HASHMAP_ATOMIC_H
#define HASHMAP_ATOMIC_H

#include <stddef.h>
#include <stdint.h>
#include <hashmap.h>
#include <libpmemobj.h>

#ifndef HASHMAP_ATOMIC_TYPE_OFFSET
#define HASHMAP_ATOMIC_TYPE_OFFSET 1000
#endif

struct hashmap_atomic;
TOID_DECLARE(struct hashmap_atomic, HASHMAP_ATOMIC_TYPE_OFFSET + 0);

int hm_atomic_check(PMEMobjpool *pop, TOID(struct hashmap_atomic) hashmap);
int hm_atomic_create(PMEMobjpool *pop, TOID(struct hashmap_atomic) *map,
		void *arg);
int hm_atomic_init(PMEMobjpool *pop, TOID(struct hashmap_atomic) hashmap);
int hm_atomic_insert(PMEMobjpool *pop, TOID(struct hashmap_atomic) hashmap,
		uint64_t key, PMEMoid value);
PMEMoid hm_atomic_remove(PMEMobjpool *pop, TOID(struct hashmap_atomic) hashmap,
		uint64_t key);
PMEMoid hm_atomic_get(PMEMobjpool *pop, TOID(struct hashmap_atomic) hashmap,
		uint64_t key);
int hm_atomic_lookup(PMEMobjpool *pop, TOID(struct hashmap_atomic) hashmap,
		uint64_t key);
int hm_atomic_foreach(PMEMobjpool *pop, TOID(struct hashmap_atomic) hashmap,
	int (*cb)(uint64_t key, PMEMoid value, void *arg), void *arg);
size_t hm_atomic_count(PMEMobjpool *pop, TOID(struct hashmap_atomic) hashmap);
int hm_atomic_cmd(PMEMobjpool *pop, TOID(struct hashmap_atomic) hashmap,
		unsigned cmd, uint64_t arg);

#endif /* HASHMAP_ATOMIC_H */
