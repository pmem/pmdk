// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018, Intel Corporation */
#ifndef HASHMAP_RP_H
#define HASHMAP_RP_H

#include <stddef.h>
#include <stdint.h>
#include <hashmap.h>
#include <libpmemobj.h>

#ifndef HASHMAP_RP_TYPE_OFFSET
#define HASHMAP_RP_TYPE_OFFSET 1008
#endif

/* Flags to indicate if insertion is being made during rebuild process */
#define HASHMAP_RP_REBUILD 1
#define HASHMAP_RP_NO_REBUILD 0
/* Initial number of entries for hashamap_rp */
#define INIT_ENTRIES_NUM_RP 16
/* Load factor to indicate resize threshold */
#define HASHMAP_RP_LOAD_FACTOR 0.5f
/* Maximum number of swaps allowed during single insertion */
#define HASHMAP_RP_MAX_SWAPS 150
/* Size of an action array used during single insertion */
#define HASHMAP_RP_MAX_ACTIONS (4 * HASHMAP_RP_MAX_SWAPS + 5)

struct hashmap_rp;
TOID_DECLARE(struct hashmap_rp, HASHMAP_RP_TYPE_OFFSET + 0);

int hm_rp_check(PMEMobjpool *pop, TOID(struct hashmap_rp) hashmap);
int hm_rp_create(PMEMobjpool *pop, TOID(struct hashmap_rp) *map, void *arg);
int hm_rp_init(PMEMobjpool *pop, TOID(struct hashmap_rp) hashmap);
int hm_rp_insert(PMEMobjpool *pop, TOID(struct hashmap_rp) hashmap,
		uint64_t key, PMEMoid value);
PMEMoid hm_rp_remove(PMEMobjpool *pop, TOID(struct hashmap_rp) hashmap,
		uint64_t key);
PMEMoid hm_rp_get(PMEMobjpool *pop, TOID(struct hashmap_rp) hashmap,
		uint64_t key);
int hm_rp_lookup(PMEMobjpool *pop, TOID(struct hashmap_rp) hashmap,
		uint64_t key);
int hm_rp_foreach(PMEMobjpool *pop, TOID(struct hashmap_rp) hashmap,
		int (*cb)(uint64_t key, PMEMoid value, void *arg), void *arg);
size_t hm_rp_count(PMEMobjpool *pop, TOID(struct hashmap_rp) hashmap);
int hm_rp_cmd(PMEMobjpool *pop, TOID(struct hashmap_rp) hashmap,
		unsigned cmd, uint64_t arg);

#endif /* HASHMAP_RP_H */
