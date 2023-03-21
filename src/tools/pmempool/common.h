/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2014-2023, Intel Corporation */

/*
 * common.h -- declarations of common functions
 */

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>

#include "queue.h"
#include "log.h"
#include "blk.h"
#include "libpmemobj.h"
#include "lane.h"
#include "ulog.h"
#include "memops.h"
#include "pmalloc.h"
#include "list.h"
#include "obj.h"
#include "memblock.h"
#include "heap_layout.h"
#include "tx.h"
#include "heap.h"
#include "btt_layout.h"
#include "page_size.h"

/* XXX - modify Linux makefiles to generate srcversion.h and remove #ifdef */
#ifdef _WIN32
#include "srcversion.h"

#define WIN_DEPR_STR "Windows support is deprecated."
#define WIN_DEPR_ATTR __declspec(deprecated(WIN_DEPR_STR))
#endif

#define COUNT_OF(x) (sizeof(x) / sizeof(0[x]))

#define OPT_SHIFT 12
#define OPT_MASK (~((1 << OPT_SHIFT) - 1))
#define OPT_LOG (1 << (PMEM_POOL_TYPE_LOG + OPT_SHIFT)) /* deprecated */
#define OPT_BLK (1 << (PMEM_POOL_TYPE_BLK + OPT_SHIFT)) /* deprecated */
#define OPT_OBJ (1 << (PMEM_POOL_TYPE_OBJ + OPT_SHIFT))
#define OPT_BTT (1 << (PMEM_POOL_TYPE_BTT + OPT_SHIFT)) /* deprecated */
#define OPT_ALL (OPT_LOG | OPT_BLK | OPT_OBJ | OPT_BTT)

#define OPT_REQ_SHIFT	8
#define OPT_REQ_MASK	((1 << OPT_REQ_SHIFT) - 1)
#define _OPT_REQ(c, n) ((c) << (OPT_REQ_SHIFT * (n)))
#define OPT_REQ0(c) _OPT_REQ(c, 0)
#define OPT_REQ1(c) _OPT_REQ(c, 1)
#define OPT_REQ2(c) _OPT_REQ(c, 2)
#define OPT_REQ3(c) _OPT_REQ(c, 3)
#define OPT_REQ4(c) _OPT_REQ(c, 4)
#define OPT_REQ5(c) _OPT_REQ(c, 5)
#define OPT_REQ6(c) _OPT_REQ(c, 6)
#define OPT_REQ7(c) _OPT_REQ(c, 7)

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

#define FOREACH_RANGE(range, ranges)\
	PMDK_LIST_FOREACH(range, &(ranges)->head, next)

#define PLIST_OFF_TO_PTR(pop, off)\
((off) == 0 ? NULL : (void *)((uintptr_t)(pop) + (off) - OBJ_OOB_SIZE))

#define ENTRY_TO_ALLOC_HDR(entry)\
((void *)((uintptr_t)(entry) - sizeof(struct allocation_header)))

#define OBJH_FROM_PTR(ptr)\
((void *)((uintptr_t)(ptr) - sizeof(struct legacy_object_header)))

#define DEFAULT_HDR_SIZE	PMEM_PAGESIZE
#define DEFAULT_DESC_SIZE	PMEM_PAGESIZE
#define POOL_HDR_DESC_SIZE	(DEFAULT_HDR_SIZE + DEFAULT_DESC_SIZE)

#define PTR_TO_ALLOC_HDR(ptr)\
((void *)((uintptr_t)(ptr) -\
	sizeof(struct legacy_object_header)))

#define OBJH_TO_PTR(objh)\
((void *)((uintptr_t)(objh) + sizeof(struct legacy_object_header)))

/* invalid answer for ask_* functions */
#define INV_ANS	'\0'

#define FORMAT_PRINTF(a, b) __attribute__((__format__(__printf__, (a), (b))))

/*
 * pmem_pool_type_t -- pool types
 */
typedef enum {
	PMEM_POOL_TYPE_LOG	= 0x01, /* deprecated */
	PMEM_POOL_TYPE_BLK	= 0x02, /* deprecated */
	PMEM_POOL_TYPE_OBJ	= 0x04,
	PMEM_POOL_TYPE_BTT	= 0x08, /* deprecated */
	PMEM_POOL_TYPE_ALL	= 0x0f,
	PMEM_POOL_TYPE_UNKNOWN	= 0x80,
} pmem_pool_type_t;

struct option_requirement {
	int opt;
	pmem_pool_type_t type;
	uint64_t req;
};

struct options {
	const struct option *opts;
	size_t noptions;
	char *bitmap;
	const struct option_requirement *req;
};

struct pmem_pool_params {
	pmem_pool_type_t type;
	char signature[POOL_HDR_SIG_LEN];
	uint64_t size;
	mode_t mode;
	int is_poolset;
	int is_part;
	int is_checksum_ok;
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
	bool fileio;
};

#ifdef _WIN32
WIN_DEPR_ATTR
#endif
struct pool_set_file *pool_set_file_open(const char *fname,
		int rdonly, int check);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
void pool_set_file_close(struct pool_set_file *file);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
int pool_set_file_read(struct pool_set_file *file, void *buff,
		size_t nbytes, uint64_t off);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
int pool_set_file_write(struct pool_set_file *file, void *buff,
		size_t nbytes, uint64_t off);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
int pool_set_file_set_replica(struct pool_set_file *file, size_t replica);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
size_t pool_set_file_nreplicas(struct pool_set_file *file);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
void *pool_set_file_map(struct pool_set_file *file, uint64_t offset);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
void pool_set_file_persist(struct pool_set_file *file,
		const void *addr, size_t len);

struct range {
	PMDK_LIST_ENTRY(range) next;
	uint64_t first;
	uint64_t last;
};

struct ranges {
	PMDK_LIST_HEAD(rangeshead, range) head;
};

#ifdef _WIN32
WIN_DEPR_ATTR
#endif
pmem_pool_type_t pmem_pool_type_parse_hdr(const struct pool_hdr *hdrp);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
pmem_pool_type_t pmem_pool_type(const void *base_pool_addr);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
int pmem_pool_checksum(const void *base_pool_addr);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
pmem_pool_type_t pmem_pool_type_parse_str(const char *str);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
uint64_t pmem_pool_get_min_size(pmem_pool_type_t type);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
int pmem_pool_parse_params(const char *fname, struct pmem_pool_params *paramsp,
		int check);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
int util_poolset_map(const char *fname, struct pool_set **poolset, int rdonly);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
struct options *util_options_alloc(const struct option *options,
		size_t nopts, const struct option_requirement *req);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
void util_options_free(struct options *opts);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
int util_options_verify(const struct options *opts, pmem_pool_type_t type);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
int util_options_getopt(int argc, char *argv[], const char *optstr,
		const struct options *opts);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
pmem_pool_type_t util_get_pool_type_second_page(const void *pool_base_addr);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
int util_parse_mode(const char *str, mode_t *mode);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
int util_parse_ranges(const char *str, struct ranges *rangesp,
		struct range entire);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
int util_ranges_add(struct ranges *rangesp, struct range range);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
void util_ranges_clear(struct ranges *rangesp);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
int util_ranges_contain(const struct ranges *rangesp, uint64_t n);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
int util_ranges_empty(const struct ranges *rangesp);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
int util_check_memory(const uint8_t *buff, size_t len, uint8_t val);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
int util_parse_chunk_types(const char *str, uint64_t *types);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
int util_parse_lane_sections(const char *str, uint64_t *types);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
char ask(char op, char *answers, char def_ans, const char *fmt, va_list ap);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
char ask_Yn(char op, const char *fmt, ...) FORMAT_PRINTF(2, 3);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
char ask_yN(char op, const char *fmt, ...) FORMAT_PRINTF(2, 3);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
unsigned util_heap_max_zone(size_t size);

#ifdef _WIN32
WIN_DEPR_ATTR
#endif
int util_pool_clear_badblocks(const char *path, int create);

static const struct range ENTIRE_UINT64 = {
	{ NULL, NULL },	/* range */
	0,		/* first */
	UINT64_MAX	/* last */
};
