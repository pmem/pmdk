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
#include "page_size.h"

#define COUNT_OF(x) (sizeof(x) / sizeof(0[x]))

#define OPT_SHIFT 12
#define OPT_MASK (~((1 << OPT_SHIFT) - 1))
#define OPT_OBJ (1 << (PMEM_POOL_TYPE_OBJ + OPT_SHIFT))
#define OPT_ALL (OPT_OBJ)

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
	PMEM_POOL_TYPE_RESERVED1 = 0x01, /* deprecated type LOG */
	PMEM_POOL_TYPE_RESERVED2 = 0x02, /* deprecated type BLK */
	PMEM_POOL_TYPE_OBJ	= 0x04,
	PMEM_POOL_TYPE_RESERVED3 = 0x08, /* deprecated type BTT */
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
	union { /* XXX to be fixed later */
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

struct pool_set_file *pool_set_file_open(const char *fname,
		int rdonly, int check);
void pool_set_file_close(struct pool_set_file *file);
int pool_set_file_read(struct pool_set_file *file, void *buff,
		size_t nbytes, uint64_t off);
int pool_set_file_write(struct pool_set_file *file, void *buff,
		size_t nbytes, uint64_t off);
int pool_set_file_set_replica(struct pool_set_file *file, size_t replica);
size_t pool_set_file_nreplicas(struct pool_set_file *file);
void *pool_set_file_map(struct pool_set_file *file, uint64_t offset);
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

pmem_pool_type_t pmem_pool_type_parse_hdr(const struct pool_hdr *hdrp);
pmem_pool_type_t pmem_pool_type(const void *base_pool_addr);
int pmem_pool_checksum(const void *base_pool_addr);
pmem_pool_type_t pmem_pool_type_parse_str(const char *str);
uint64_t pmem_pool_get_min_size(pmem_pool_type_t type);
int pmem_pool_parse_params(const char *fname, struct pmem_pool_params *paramsp,
		int check);
int util_poolset_map(const char *fname, struct pool_set **poolset, int rdonly);
struct options *util_options_alloc(const struct option *options,
		size_t nopts, const struct option_requirement *req);
void util_options_free(struct options *opts);
int util_options_verify(const struct options *opts, pmem_pool_type_t type);
int util_options_getopt(int argc, char *argv[], const char *optstr,
		const struct options *opts);
int util_parse_mode(const char *str, mode_t *mode);
int util_parse_ranges(const char *str, struct ranges *rangesp,
		struct range entire);
int util_ranges_add(struct ranges *rangesp, struct range range);
void util_ranges_clear(struct ranges *rangesp);
int util_ranges_contain(const struct ranges *rangesp, uint64_t n);
int util_ranges_empty(const struct ranges *rangesp);
int util_check_memory(const uint8_t *buff, size_t len, uint8_t val);
int util_parse_chunk_types(const char *str, uint64_t *types);
int util_parse_lane_sections(const char *str, uint64_t *types);
char ask(char op, char *answers, char def_ans, const char *fmt, va_list ap);
char ask_Yn(char op, const char *fmt, ...) FORMAT_PRINTF(2, 3);
char ask_yN(char op, const char *fmt, ...) FORMAT_PRINTF(2, 3);
unsigned util_heap_max_zone(size_t size);

int util_pool_clear_badblocks(const char *path, int create);

static const struct range ENTIRE_UINT64 = {
	{ NULL, NULL },	/* range */
	0,		/* first */
	UINT64_MAX	/* last */
};
