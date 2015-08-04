/*
 * Copyright (c) 2015, Intel Corporation
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
 *     * Neither the name of Intel Corporation nor the names of its
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
 * tree_map.c -- TreeMap sorted collection implementation
 */

#ifndef	TREE_MAP_H
#define	TREE_MAP_H

#include <libpmemobj.h>

#define	TREE_MAP_TYPE_OFFSET 1000
TOID_DECLARE(struct tree_map, TREE_MAP_TYPE_OFFSET + 0);

struct tree_map;

int tree_map_new(PMEMobjpool *pop, TOID(struct tree_map) *map);

int tree_map_delete(PMEMobjpool *pop, TOID(struct tree_map) *map);

int tree_map_insert(PMEMobjpool *pop,
	TOID(struct tree_map) map, uint64_t key, PMEMoid value);

int tree_map_insert_new(PMEMobjpool *pop,
	TOID(struct tree_map) map, uint64_t key, size_t size,
		unsigned int type_num,
		void (*constructor)(PMEMobjpool *pop, void *ptr, void *arg),
		void *arg);

PMEMoid tree_map_remove(PMEMobjpool *pop,
	TOID(struct tree_map) map, uint64_t key);

int tree_map_remove_free(PMEMobjpool *pop,
	TOID(struct tree_map) map, uint64_t key);

int tree_map_clear(PMEMobjpool *pop,
	TOID(struct tree_map) map);

PMEMoid tree_map_get(TOID(struct tree_map) map, uint64_t key);

int tree_map_foreach(TOID(struct tree_map) map,
	int (*cb)(uint64_t key, PMEMoid value, void *arg), void *arg);

int tree_map_is_empty(TOID(struct tree_map) map);

#endif /* TREE_MAP_H */
