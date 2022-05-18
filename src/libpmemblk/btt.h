/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2014-2022, Intel Corporation */

/*
 * btt.h -- btt module definitions
 */

#ifndef BTT_H
#define BTT_H 1

#ifdef __cplusplus
extern "C" {
#endif

/* callback functions passed to btt_init() */
struct ns_callback {
	int (*nsread)(void *ns, unsigned lane,
		void *buf, size_t count, uint64_t off);
	int (*nswrite)(void *ns, unsigned lane,
		const void *buf, size_t count, uint64_t off);
	int (*nszero)(void *ns, unsigned lane, size_t count, uint64_t off);
	ssize_t (*nsmap)(void *ns, unsigned lane, void **addrp,
			size_t len, uint64_t off);
	void (*nssync)(void *ns, unsigned lane, void *addr, size_t len);

	int ns_is_zeroed;
};

struct btt_info;

struct btt *btt_init(uint64_t rawsize, uint32_t lbasize, uint8_t parent_uuid[],
		unsigned maxlane, void *ns, const struct ns_callback *ns_cbp);
unsigned btt_nlane(struct btt *bttp);
size_t btt_nlba(struct btt *bttp);
int btt_read(struct btt *bttp, unsigned lane, uint64_t lba, void *buf);
int btt_write(struct btt *bttp, unsigned lane, uint64_t lba, const void *buf);
int btt_set_zero(struct btt *bttp, unsigned lane, uint64_t lba);
int btt_set_error(struct btt *bttp, unsigned lane, uint64_t lba);
int btt_check(struct btt *bttp);
void btt_fini(struct btt *bttp);

uint64_t btt_flog_size(uint32_t nfree);
uint64_t btt_map_size(uint32_t external_nlba);
uint64_t btt_arena_datasize(uint64_t arena_size, uint32_t nfree);
int btt_info_set(struct btt_info *info, uint32_t external_lbasize,
	uint32_t nfree, uint64_t arena_size, uint64_t space_left);

struct btt_flog *btt_flog_get_valid(struct btt_flog *flog_pair, int *next);
int map_entry_is_initial(uint32_t map_entry);
void btt_info_convert2h(struct btt_info *infop);
void btt_info_convert2le(struct btt_info *infop);
void btt_flog_convert2h(struct btt_flog *flogp);
void btt_flog_convert2le(struct btt_flog *flogp);

#ifdef __cplusplus
}
#endif

#endif
