/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2015-2020, Intel Corporation */

/*
 * map.h -- common interface for maps
 */

#ifndef MAP_H
#define MAP_H

#include <libpmemobj.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MAP_TYPE_OFFSET
#define MAP_TYPE_OFFSET 1000
#endif

TOID_DECLARE(struct map, MAP_TYPE_OFFSET + 0);

struct map;
struct map_ctx;

struct map_ops {
	int(*check)(PMEMobjpool *pop, TOID(struct map) map);
	int(*create)(PMEMobjpool *pop, TOID(struct map) *map, void *arg);
	int(*destroy)(PMEMobjpool *pop, TOID(struct map) *map);
	int(*init)(PMEMobjpool *pop, TOID(struct map) map);
	int(*insert)(PMEMobjpool *pop, TOID(struct map) map,
		uint64_t key, PMEMoid value);
	int(*insert_new)(PMEMobjpool *pop, TOID(struct map) map,
		uint64_t key, size_t size,
		unsigned type_num,
		void(*constructor)(PMEMobjpool *pop, void *ptr, void *arg),
		void *arg);
	PMEMoid(*remove)(PMEMobjpool *pop, TOID(struct map) map,
		uint64_t key);
	int(*remove_free)(PMEMobjpool *pop, TOID(struct map) map,
		uint64_t key);
	int(*clear)(PMEMobjpool *pop, TOID(struct map) map);
	PMEMoid(*get)(PMEMobjpool *pop, TOID(struct map) map,
		uint64_t key);
	int(*lookup)(PMEMobjpool *pop, TOID(struct map) map,
		uint64_t key);
	int(*foreach)(PMEMobjpool *pop, TOID(struct map) map,
		int(*cb)(uint64_t key, PMEMoid value, void *arg),
		void *arg);
	int(*is_empty)(PMEMobjpool *pop, TOID(struct map) map);
	size_t(*count)(PMEMobjpool *pop, TOID(struct map) map);
	int(*cmd)(PMEMobjpool *pop, TOID(struct map) map,
		unsigned cmd, uint64_t arg);
};

struct map_ctx {
	PMEMobjpool *pop;
	const struct map_ops *ops;
};

struct map_ctx *map_ctx_init(const struct map_ops *ops, PMEMobjpool *pop);
void map_ctx_free(struct map_ctx *mapc);
int map_check(struct map_ctx *mapc, TOID(struct map) map);
int map_create(struct map_ctx *mapc, TOID(struct map) *map, void *arg);
int map_destroy(struct map_ctx *mapc, TOID(struct map) *map);
int map_init(struct map_ctx *mapc, TOID(struct map) map);
int map_insert(struct map_ctx *mapc, TOID(struct map) map,
	uint64_t key, PMEMoid value);
int map_insert_new(struct map_ctx *mapc, TOID(struct map) map,
	uint64_t key, size_t size,
	unsigned type_num,
	void(*constructor)(PMEMobjpool *pop, void *ptr, void *arg),
	void *arg);
PMEMoid map_remove(struct map_ctx *mapc, TOID(struct map) map, uint64_t key);
int map_remove_free(struct map_ctx *mapc, TOID(struct map) map, uint64_t key);
int map_clear(struct map_ctx *mapc, TOID(struct map) map);
PMEMoid map_get(struct map_ctx *mapc, TOID(struct map) map, uint64_t key);
int map_lookup(struct map_ctx *mapc, TOID(struct map) map, uint64_t key);
int map_foreach(struct map_ctx *mapc, TOID(struct map) map,
	int(*cb)(uint64_t key, PMEMoid value, void *arg),
	void *arg);
int map_is_empty(struct map_ctx *mapc, TOID(struct map) map);
size_t map_count(struct map_ctx *mapc, TOID(struct map) map);
int map_cmd(struct map_ctx *mapc, TOID(struct map) map,
	unsigned cmd, uint64_t arg);

#ifdef __cplusplus
}
#endif

#endif /* MAP_H */
