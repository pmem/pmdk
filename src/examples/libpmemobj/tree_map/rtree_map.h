/*
 * Copyright 2016, Intel Corporation
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

/*
 * rtree_map.h -- Radix TreeMap collection implementation
 */

#ifndef RTREE_MAP_H
#define RTREE_MAP_H

#include <libpmemobj.h>

#ifndef RTREE_MAP_TYPE_OFFSET
#define RTREE_MAP_TYPE_OFFSET 1020
#endif

struct rtree_map;
TOID_DECLARE(struct rtree_map, RTREE_MAP_TYPE_OFFSET + 0);

int rtree_map_check(PMEMobjpool *pop, TOID(struct rtree_map) map);
int rtree_map_create(PMEMobjpool *pop, TOID(struct rtree_map) *map, void *arg);
int rtree_map_destroy(PMEMobjpool *pop, TOID(struct rtree_map) *map);
int rtree_map_insert(PMEMobjpool *pop, TOID(struct rtree_map) map,
	const unsigned char *key, uint64_t key_size, PMEMoid value);
int rtree_map_insert_new(PMEMobjpool *pop, TOID(struct rtree_map) map,
		const unsigned char *key, uint64_t key_size,
		size_t size, unsigned type_num,
		void (*constructor)(PMEMobjpool *pop, void *ptr, void *arg),
		void *arg);
PMEMoid rtree_map_remove(PMEMobjpool *pop, TOID(struct rtree_map) map,
		const unsigned char *key, uint64_t key_size);
int rtree_map_remove_free(PMEMobjpool *pop, TOID(struct rtree_map) map,
		const unsigned char *key, uint64_t key_size);
int rtree_map_clear(PMEMobjpool *pop, TOID(struct rtree_map) map);
PMEMoid rtree_map_get(PMEMobjpool *pop, TOID(struct rtree_map) map,
		const unsigned char *key, uint64_t key_size);
int rtree_map_lookup(PMEMobjpool *pop, TOID(struct rtree_map) map,
		const unsigned char *key, uint64_t key_size);
int rtree_map_foreach(PMEMobjpool *pop, TOID(struct rtree_map) map,
	int (*cb)(const unsigned char *key, uint64_t key_size,
		PMEMoid value, void *arg),
	void *arg);
int rtree_map_is_empty(PMEMobjpool *pop, TOID(struct rtree_map) map);

#endif /* RTREE_MAP_H */
