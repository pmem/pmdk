/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2014-2023, Intel Corporation */

/*
 * info.h -- pmempool info command header file
 */

#include "vec.h"

#define BLK_DEPR_STR "Libpmemblk is deprecated."
#define LOG_DEPR_STR "Libpmemlog is deprecated."
#define PMEMBLK_DEPR_ATTR __attribute__((deprecated(BLK_DEPR_STR)))
#define PMEMLOG_DEPR_ATTR __attribute__((deprecated(LOG_DEPR_STR)))

/*
 * Verbose levels used in application:
 *
 * VERBOSE_DEFAULT:
 * Default value for application's verbosity level.
 * This is also set for data structures which should be
 * printed without any command line argument.
 *
 * VERBOSE_MAX:
 * Maximum value for application's verbosity level.
 * This value is used when -v command line argument passed.
 *
 * VERBOSE_SILENT:
 * This value is higher than VERBOSE_MAX and it is used only
 * for verbosity levels of data structures which should _not_ be
 * printed without specified command line arguments.
 */
#define VERBOSE_SILENT	0
#define VERBOSE_DEFAULT	1
#define VERBOSE_MAX	2

/*
 * print_bb_e -- printing bad blocks options
 */
enum print_bb_e {
	PRINT_BAD_BLOCKS_NOT_SET,
	PRINT_BAD_BLOCKS_NO,
	PRINT_BAD_BLOCKS_YES,

	PRINT_BAD_BLOCKS_MAX
};

/*
 * pmempool_info_args -- structure for storing command line arguments
 */
struct pmempool_info_args {
	char *file;		/* input file */
	unsigned col_width;	/* column width for printing fields */
	bool human;		/* sizes in human-readable formats */
	bool force;		/* force parsing pool */
	enum print_bb_e badblocks; /* print bad blocks */
	pmem_pool_type_t type;	/* forced pool type */
	bool use_range;		/* use range for blocks */
	struct ranges ranges;	/* range of block/chunks to dump */
	int vlevel;		/* verbosity level */
	int vdata;		/* verbosity level for data dump */
	int vhdrdump;		/* verbosity level for headers hexdump */
	int vstats;		/* verbosity level for statistics */
	struct {
		size_t walk;		/* data chunk size */
	} log; /* deprecated */
	struct {
		int vmap;	/* verbosity level for BTT Map */
		int vflog;	/* verbosity level for BTT FLOG */
		int vbackup;	/* verbosity level for BTT Info backup */
		bool skip_zeros; /* skip blocks marked with zero flag */
		bool skip_error; /* skip blocks marked with error flag */
		bool skip_no_flag; /* skip blocks not marked with any flag */
	} blk; /* deprecated */
	struct {
		int vlanes;		/* verbosity level for lanes */
		int vroot;
		int vobjects;
		int valloc;
		int voobhdr;
		int vheap;
		int vzonehdr;
		int vchunkhdr;
		int vbitmap;
		bool lanes_recovery;
		bool ignore_empty_obj;
		uint64_t chunk_types;
		size_t replica;
		struct ranges lane_ranges;
		struct ranges type_ranges;
		struct ranges zone_ranges;
		struct ranges chunk_ranges;
	} obj;
};

/*
 * pmem_blk_stats -- structure with statistics for pmemblk (DEPRECATED)
 */
struct pmem_blk_stats {
	uint32_t total;		/* number of processed blocks */
	uint32_t zeros;		/* number of blocks marked by zero flag */
	uint32_t errors;	/* number of blocks marked by error flag */
	uint32_t noflag;	/* number of blocks not marked with any flag */
};

struct pmem_obj_class_stats {
	uint64_t n_units;
	uint64_t n_used;
	uint64_t unit_size;
	uint64_t alignment;
	uint32_t nallocs;
	uint16_t flags;
};

struct pmem_obj_zone_stats {
	uint64_t n_chunks;
	uint64_t n_chunks_type[MAX_CHUNK_TYPE];
	uint64_t size_chunks;
	uint64_t size_chunks_type[MAX_CHUNK_TYPE];
	VEC(, struct pmem_obj_class_stats) class_stats;
};

struct pmem_obj_type_stats {
	PMDK_TAILQ_ENTRY(pmem_obj_type_stats) next;
	uint64_t type_num;
	uint64_t n_objects;
	uint64_t n_bytes;
};

struct pmem_obj_stats {
	uint64_t n_total_objects;
	uint64_t n_total_bytes;
	uint64_t n_zones;
	uint64_t n_zones_used;
	struct pmem_obj_zone_stats *zone_stats;
	PMDK_TAILQ_HEAD(obj_type_stats_head, pmem_obj_type_stats) type_stats;
};

/*
 * pmem_info -- context for pmeminfo application
 */
struct pmem_info {
	const char *file_name;	/* current file name */
	struct pool_set_file *pfile;
	struct pmempool_info_args args;	/* arguments parsed from command line */
	struct options *opts;
	struct pool_set *poolset;
	pmem_pool_type_t type;
	struct pmem_pool_params params;
	struct {
		struct pmem_blk_stats stats;
	} blk; /* deprecated */
	struct {
		struct pmemobjpool *pop;
		struct palloc_heap *heap;
		struct alloc_class_collection *alloc_classes;
		size_t size;
		struct pmem_obj_stats stats;
		uint64_t uuid_lo;
		uint64_t objid;
	} obj;
};

int pmempool_info_func(const char *appname, int argc, char *argv[]);
void pmempool_info_help(const char *appname);

int pmempool_info_read(struct pmem_info *pip, void *buff,
		size_t nbytes, uint64_t off);
PMEMBLK_DEPR_ATTR int pmempool_info_blk(struct pmem_info *pip);
PMEMLOG_DEPR_ATTR int pmempool_info_log(struct pmem_info *pip);
int pmempool_info_obj(struct pmem_info *pip);
PMEMBLK_DEPR_ATTR int pmempool_info_btt(struct pmem_info *pip);
