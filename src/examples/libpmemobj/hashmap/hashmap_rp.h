/*
 * Copyright 2018, Intel Corporation
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
