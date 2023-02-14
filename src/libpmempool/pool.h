/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2016-2023, Intel Corporation */

/*
 * pool.h -- internal definitions for pool processing functions
 */

#ifndef POOL_H
#define POOL_H

#include <stdbool.h>
#include <sys/types.h>

#include "libpmemobj.h"

#include "queue.h"
#include "set.h"
#include "log.h"
#include "blk.h"
#include "btt_layout.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "alloc.h"
#include "fault_injection.h"

enum pool_type {
	POOL_TYPE_UNKNOWN	= (1 << 0),
	POOL_TYPE_LOG		= (1 << 1),
	POOL_TYPE_BLK		= (1 << 2), // DEPRECATED
	POOL_TYPE_OBJ		= (1 << 3),
	POOL_TYPE_BTT		= (1 << 4),

	POOL_TYPE_ANY		= POOL_TYPE_UNKNOWN | POOL_TYPE_LOG |
		POOL_TYPE_BLK | POOL_TYPE_OBJ | POOL_TYPE_BTT,
};

struct pool_params {
	enum pool_type type;
	char signature[POOL_HDR_SIG_LEN];
	features_t features;
	size_t size;
	mode_t mode;
	int is_poolset;
	int is_part;
	int is_dev_dax;
	int is_pmem;
	union {
		struct {
			uint64_t bsize;
		} blk;
		struct {
			char layout[PMEMOBJ_MAX_LAYOUT];
		} obj;
	};
};

struct pool_set_file {
	int fd;
	char *fname;
	void *addr;
	size_t size;
	struct pool_set *poolset;
	time_t mtime;
	mode_t mode;
};

struct arena {
	PMDK_TAILQ_ENTRY(arena) next;
	struct btt_info btt_info;
	uint32_t id;
	bool valid;
	bool zeroed;
	uint64_t offset;
	uint8_t *flog;
	size_t flogsize;
	uint32_t *map;
	size_t mapsize;
};

struct pool_data {
	struct pool_params params;
	struct pool_set_file *set_file;
	int blk_no_layout;
	union {
		struct pool_hdr pool;
		struct pmemlog log;
		struct pmemblk blk;
	} hdr;
	enum {
		UUID_NOP = 0,
		UUID_FROM_BTT,
		UUID_NOT_FROM_BTT,
	} uuid_op;
	struct arena bttc;
	PMDK_TAILQ_HEAD(arenashead, arena) arenas;
	uint32_t narenas;
};

struct pool_data *pool_data_alloc(PMEMpoolcheck *ppc);
void pool_data_free(struct pool_data *pool);
void pool_params_from_header(struct pool_params *params,
	const struct pool_hdr *hdr);

int pool_set_parse(struct pool_set **setp, const char *path);
void *pool_set_file_map(struct pool_set_file *file, uint64_t offset);
int pool_read(struct pool_data *pool, void *buff, size_t nbytes,
	uint64_t off);
int pool_write(struct pool_data *pool, const void *buff, size_t nbytes,
	uint64_t off);
int pool_copy(struct pool_data *pool, const char *dst_path, int overwrite);
int pool_set_part_copy(struct pool_set_part *dpart,
	struct pool_set_part *spart, int overwrite);
int pool_memset(struct pool_data *pool, uint64_t off, int c, size_t count);

unsigned pool_set_files_count(struct pool_set_file *file);
int pool_set_file_map_headers(struct pool_set_file *file, int rdonly, int prv);
void pool_set_file_unmap_headers(struct pool_set_file *file);

void pool_hdr_default(enum pool_type type, struct pool_hdr *hdrp);
enum pool_type pool_hdr_get_type(const struct pool_hdr *hdrp);
enum pool_type pool_set_type(struct pool_set *set);
const char *pool_get_pool_type_str(enum pool_type type);

int pool_btt_info_valid(struct btt_info *infop);

int pool_blk_get_first_valid_arena(struct pool_data *pool,
	struct arena *arenap);
int pool_blk_bsize_valid(uint32_t bsize, uint64_t fsize);
uint64_t pool_next_arena_offset(struct pool_data *pool, uint64_t header_offset);
uint64_t pool_get_first_valid_btt(struct pool_data *pool,
	struct btt_info *infop, uint64_t offset, bool *zeroed);
size_t pool_get_min_size(enum pool_type);

#if FAULT_INJECTION
void
pmempool_inject_fault_at(enum pmem_allocation_type type, int nth,
							const char *at);

int
pmempool_fault_injection_enabled(void);
#else
static inline void
pmempool_inject_fault_at(enum pmem_allocation_type type, int nth,
						const char *at)
{
	/* suppress unused-parameter errors */
	SUPPRESS_UNUSED(type, nth, at);

	abort();
}

static inline int
pmempool_fault_injection_enabled(void)
{
	return 0;
}
#endif

#ifdef __cplusplus
}
#endif

#endif
