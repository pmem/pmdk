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
 * pool.h -- internal definitions for pool processing functions
 */

#include <stdbool.h>
#include <sys/types.h>
#include <sys/queue.h>

#include "util.h"
#include "log.h"
#include "blk.h"
#include "btt_layout.h"
#include "libpmemobj.h"

enum pool_type {
	POOL_TYPE_LOG		= 0x01,
	POOL_TYPE_BLK		= 0x02,
	POOL_TYPE_OBJ		= 0x04,
	POOL_TYPE_ALL		= 0x0f,
	POOL_TYPE_UNKNOWN	= 0x0f,
	POOL_TYPE_NONE		= 0x10,
};

struct pool_params {
	enum pool_type type;
	char signature[POOL_HDR_SIG_LEN];
	uint64_t size;
	mode_t mode;
	int is_poolset;
	int is_part;
	int is_btt_dev;
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
	size_t replica;
	time_t mtime;
	mode_t mode;
};

struct arena {
	TAILQ_ENTRY(arena) next;
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
		UUID_REGENERATED,
	} uuid_op;
	struct arena bttc;
	TAILQ_HEAD(arenashead, arena) arenas;
	uint32_t narenas;
};

struct pool_data *pool_data_alloc(PMEMpoolcheck *ppc);
void pool_data_free(struct pool_data *pool);
void
pool_params_from_header(struct pool_params *params, const struct pool_hdr *hdr);

void *pool_set_file_map(struct pool_set_file *file, uint64_t offset);
int pool_read(struct pool_data *pool, void *buff, size_t nbytes,
	uint64_t off);
int pool_write(struct pool_data *pool, const void *buff, size_t nbytes,
	uint64_t off);
int pool_copy(struct pool_data *pool, const char *dst_path);
int pool_memset(struct pool_data *pool, uint64_t off, int c, size_t count);

unsigned pool_set_files_count(struct pool_set_file *file);
int pool_set_file_map_headers(struct pool_set_file *file, int rdonly,
	size_t hdrsize);
void pool_set_file_unmap_headers(struct pool_set_file *file);

void pool_hdr_convert2h(struct pool_hdr *hdrp);
void pool_hdr_convert2le(struct pool_hdr *hdrp);
void pool_hdr_default(enum pool_type type, struct pool_hdr *hdrp);
enum pool_type pool_hdr_get_type(const struct pool_hdr *hdrp);

void pool_btt_info_convert2h(struct btt_info *infop);
void pool_btt_info_convert2le(struct btt_info *infop);
int pool_btt_info_valid(struct btt_info *infop);

int
pool_blk_get_first_valid_arena(struct pool_data *pool, struct arena *arenap);
uint64_t pool_next_arena_offset(struct pool_data *pool, uint64_t header_offset);
uint64_t
pool_get_first_valid_btt(struct pool_data *pool, struct btt_info *infop,
	uint64_t offset, bool *zeroed);
