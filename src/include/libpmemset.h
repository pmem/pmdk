/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2020-2021, Intel Corporation */

/*
 * libpmemset.h -- definitions of libpmemset entry points (EXPERIMENTAL)
 *
 * See libpmemset(7) for details.
 */

#ifndef LIBPMEMSET_H
#define LIBPMEMSET_H 1

#include <stdbool.h>
#include <stddef.h>
#include <libpmem2.h>

#ifdef _WIN32
#include <pmemcompat.h>

#ifndef PMDK_UTF8_API
#define pmemset_source_from_file pmemset_source_from_fileW
#define pmemset_xsource_from_file pmemset_xsource_from_fileW
#define pmemset_source_from_temporary pmemset_source_from_temporaryW
#define pmemset_errormsg pmemset_errormsgW
#define pmemset_perror pmemset_perrorW
#else
#define pmemset_source_from_file pmemset_source_from_fileU
#define pmemset_xsource_from_file pmemset_xsource_from_fileU
#define pmemset_source_from_temporary pmemset_source_from_temporaryU
#define pmemset_errormsg pmemset_errormsgU
#define pmemset_perror pmemset_perrorU
#endif

#endif

#ifdef __cplusplus
extern "C" {
#endif

/* pmemset errors */

#define PMEMSET_E_UNKNOWN				(-200000)
#define PMEMSET_E_NOSUPP				(-200001)
#define PMEMSET_E_INVALID_PMEM2_SOURCE			(-200002)
#define PMEMSET_E_INVALID_SOURCE_PATH			(-200003)
#define PMEMSET_E_INVALID_SOURCE_TYPE			(-200004)
#define PMEMSET_E_CANNOT_ALLOCATE_INTERNAL_STRUCTURE	(-200005)
#define PMEMSET_E_INVALID_OFFSET_VALUE			(-200006)
#define PMEMSET_E_GRANULARITY_NOT_SUPPORTED		(-200007)
#define PMEMSET_E_INVALID_PMEM2_MAP			(-200008)
#define PMEMSET_E_PART_EXISTS				(-200009)
#define PMEMSET_E_GRANULARITY_NOT_SET			(-200010)
#define PMEMSET_E_GRANULARITY_MISMATCH			(-200011)
#define PMEMSET_E_NO_PART_MAPPED			(-200012)
#define PMEMSET_E_CANNOT_FIND_PART_MAP			(-200013)
#define PMEMSET_E_CANNOT_COALESCE_PARTS			(-200014)
#define PMEMSET_E_LENGTH_UNALIGNED			(-200015)
#define PMEMSET_E_PART_NOT_FOUND			(-200016)
#define PMEMSET_E_INVALID_COALESCING_VALUE		(-200017)
#define PMEMSET_E_DEEP_FLUSH_FAIL			(-200018)
#define PMEMSET_E_INVALID_SOURCE_FILE_CREATE_FLAGS	(-200019)
#define PMEMSET_E_CANNOT_CREATE_TEMP_FILE		(-200020)
#define PMEMSET_E_CANNOT_GROW_SOURCE_FILE		(-200021)
#define PMEMSET_E_PART_MAP_POSSIBLE_USE_AFTER_DROP	(-200022)
#define PMEMSET_E_CANNOT_FIT_PART_MAP			(-200023)
#define PMEMSET_E_OFFSET_OUT_OF_RANGE			(-200024)
#define PMEMSET_E_INVALID_PART_STATES			(-200025)
#define PMEMSET_E_UNDESIRABLE_PART_STATE		(-200026)
#define PMEMSET_E_SDS_ALREADY_SET			(-200027)
#define PMEMSET_E_SDS_ENOSUPP				(-200028)
#define PMEMSET_E_SDS_DEVICE_ID_LEN_TOO_BIG		(-200029)
#define PMEMSET_E_MAP_LENGTH_UNSET			(-200030)
#define PMEMSET_E_SOURCE_FILE_IS_TOO_SMALL  (-200031)

struct pmemset;
struct pmemset_config;
struct pmemset_map_config;
struct pmem2_source;
struct pmemset_source;
struct pmemset_part_map;

struct pmemset_part_descriptor {
	void *addr;
	size_t size;
};

/* pmemset event */

enum pmemset_event {
	PMEMSET_EVENT_COPY,
	PMEMSET_EVENT_MOVE,
	PMEMSET_EVENT_SET,
	PMEMSET_EVENT_FLUSH,
	PMEMSET_EVENT_DRAIN,
	PMEMSET_EVENT_PERSIST,
	PMEMSET_EVENT_BAD_BLOCK,
	PMEMSET_EVENT_REMOVE_RANGE,
	PMEMSET_EVENT_PART_ADD,
	PMEMSET_EVENT_PART_REMOVE,
	PMEMSET_EVENT_SDS_UPDATE,
};

struct pmemset_event_copy {
	void *src;
	void *dest;
	size_t len;
	unsigned flags;
};

struct pmemset_event_move {
	void *src;
	void *dest;
	size_t len;
	unsigned flags;
};

struct pmemset_event_set {
	void *dest;
	int value;
	size_t len;
	unsigned flags;
};

struct pmemset_event_flush {
	void *addr;
	size_t len;
};

struct pmemset_event_persist {
	void *addr;
	size_t len;
};

struct pmemset_event_bad_block {
	void *addr;
	size_t len;
};

struct pmemset_event_remove_range {
	void *addr;
	size_t len;
};

struct pmemset_event_part_remove {
	void *addr;
	size_t len;
};

struct pmemset_event_part_add {
	void *addr;
	size_t len;
	struct pmem2_source *src;
};

struct pmemset_event_sds_update {
	struct pmemset_sds *sds;
	struct pmemset_source *src;
};

#define PMEMSET_EVENT_CONTEXT_SIZE (64)

struct pmemset_event_context {
	enum pmemset_event type;
	union {
		char _data[PMEMSET_EVENT_CONTEXT_SIZE];
		struct pmemset_event_copy copy;
		struct pmemset_event_move move;
		struct pmemset_event_set set;
		struct pmemset_event_flush flush;
		struct pmemset_event_persist persist;
		struct pmemset_event_bad_block bad_block;
		struct pmemset_event_remove_range remove_range;
		struct pmemset_event_part_remove part_remove;
		struct pmemset_event_part_add part_add;
		struct pmemset_event_sds_update sds_update;
	} data;
};

/*
 * This callback can be used to create a copy of the data or directly
 * replicate it somewhere. This is *not* an append-only log, nor is the
 * data versioned in any way. Once the function exits, the memory range
 * can no longer be accessed.
 * There's no guarantee that accessing the data inside of the callback
 * is thread-safe. The library user must guarantee this by not
 * having multiple threads mutating the same region on the set.
 */
typedef int pmemset_event_callback(struct pmemset *set,
		struct pmemset_event_context *ctx, void *arg);

/* pmemset config setup */

int pmemset_config_new(struct pmemset_config **cfg);

void pmemset_config_set_reservation(struct pmemset_config *cfg,
		struct pmem2_vm_reservation *rsv);

int pmemset_config_set_acceptable_states(struct pmemset_config *cfg,
		uint64_t states);

void pmemset_config_set_event_callback(struct pmemset_config *config,
		pmemset_event_callback *callback, void *arg);

int pmemset_config_set_required_store_granularity(struct pmemset_config *cfg,
		enum pmem2_granularity g);

int pmemset_get_store_granularity(struct pmemset *set,
		enum pmem2_granularity *g);

int pmemset_config_delete(struct pmemset_config **cfg);

/* pmemset new */

int pmemset_new(struct pmemset **set, struct pmemset_config *cfg);

int pmemset_delete(struct pmemset **set);

/* map config setup */

int pmemset_map_config_new(struct pmemset_map_config **map_cfg, struct
		pmemset *set);

int pmemset_map_config_set_offset(struct pmemset_map_config *map_cfg,
		size_t offset);

void pmemset_map_config_set_length(struct pmemset_map_config *map_cfg,
		size_t length);

int pmemset_map_config_delete(struct pmemset_map_config **map_cfg);

/* source from file flags */

#define PMEMSET_SOURCE_FILE_CREATE_ALWAYS		(1U << 0)
#define PMEMSET_SOURCE_FILE_CREATE_IF_NEEDED	(1U << 1)
#define PMEMSET_SOURCE_FILE_DO_NOT_GROW		    (1U << 2)

#define FILE_CREATE_MODE_FROM_FLAG(flag) ((uint32_t)((flag) >> 32))
#define PMEMSET_SOURCE_FILE_CREATE_MODE(mode) (((uint64_t)(mode)) << 32)
#define PMEMSET_SOURCE_FILE_CREATE_ALWAYS_MODE(mode) \
		PMEMSET_SOURCE_FILE_CREATE_ALWAYS | \
		PMEMSET_SOURCE_FILE_CREATE_MODE(mode)

#define PMEMSET_SOURCE_FILE_CREATE_VALID_FLAGS \
		(PMEMSET_SOURCE_FILE_CREATE_ALWAYS | \
		PMEMSET_SOURCE_FILE_CREATE_IF_NEEDED | \
		PMEMSET_SOURCE_FILE_DO_NOT_GROW | \
		PMEMSET_SOURCE_FILE_VALID_MODES)

#ifndef WIN32
#define PMEMSET_SOURCE_FILE_RWXU_MODE S_IRWXU
#define PMEMSET_SOURCE_FILE_RWU_MODE S_IRUSR | S_IWUSR
#define PMEMSET_SOURCE_FILE_RUSR_MODE S_IRUSR
#define PMEMSET_SOURCE_FILE_WUSR_MODE S_IWUSR
#define PMEMSET_SOURCE_FILE_XUSR_MODE S_IXUSR
#define PMEMSET_SOURCE_FILE_RWXG_MODE S_IRWXG
#define PMEMSET_SOURCE_FILE_RWG_MODE S_IRGRP | S_IWGRP
#define PMEMSET_SOURCE_FILE_RGRP_MODE S_IRGRP
#define PMEMSET_SOURCE_FILE_WGRP_MODE S_IWGRP
#define PMEMSET_SOURCE_FILE_XGRP_MODE S_IXGRP
#define PMEMSET_SOURCE_FILE_RWXO_MODE S_IRWXO
#define PMEMSET_SOURCE_FILE_RWO_MODE S_IROTH | S_IWOTH
#define PMEMSET_SOURCE_FILE_ROTH_MODE S_IROTH
#define PMEMSET_SOURCE_FILE_WOTH_MODE S_IWOTH
#define PMEMSET_SOURCE_FILE_XOTH_MODE S_IXOTH
#else
#define PMEMSET_SOURCE_FILE_RWXU_MODE 0
#define PMEMSET_SOURCE_FILE_RWU_MODE 0
#define PMEMSET_SOURCE_FILE_RUSR_MODE 0
#define PMEMSET_SOURCE_FILE_WUSR_MODE 0
#define PMEMSET_SOURCE_FILE_XUSR_MODE 0
#define PMEMSET_SOURCE_FILE_RWXG_MODE 0
#define PMEMSET_SOURCE_FILE_RWG_MODE 0
#define PMEMSET_SOURCE_FILE_RGRP_MODE 0
#define PMEMSET_SOURCE_FILE_WGRP_MODE 0
#define PMEMSET_SOURCE_FILE_XGRP_MODE 0
#define PMEMSET_SOURCE_FILE_RWXO_MODE 0
#define PMEMSET_SOURCE_FILE_RWO_MODE 0
#define PMEMSET_SOURCE_FILE_ROTH_MODE 0
#define PMEMSET_SOURCE_FILE_WOTH_MODE 0
#define PMEMSET_SOURCE_FILE_XOTH_MODE 0
#endif

#define PMEMSET_SOURCE_FILE_VALID_MODES \
		((uint64_t)(PMEMSET_SOURCE_FILE_RWXU_MODE | \
		PMEMSET_SOURCE_FILE_RWU_MODE | \
		PMEMSET_SOURCE_FILE_RUSR_MODE | \
		PMEMSET_SOURCE_FILE_WUSR_MODE | \
		PMEMSET_SOURCE_FILE_XUSR_MODE | \
		PMEMSET_SOURCE_FILE_RWXG_MODE | \
		PMEMSET_SOURCE_FILE_RWG_MODE | \
		PMEMSET_SOURCE_FILE_RGRP_MODE | \
		PMEMSET_SOURCE_FILE_WGRP_MODE | \
		PMEMSET_SOURCE_FILE_XGRP_MODE | \
		PMEMSET_SOURCE_FILE_RWXO_MODE | \
		PMEMSET_SOURCE_FILE_RWO_MODE | \
		PMEMSET_SOURCE_FILE_ROTH_MODE | \
		PMEMSET_SOURCE_FILE_WOTH_MODE | \
		PMEMSET_SOURCE_FILE_XOTH_MODE) << 32)

/* source setup */

int pmemset_source_from_pmem2(struct pmemset_source **src,
		struct pmem2_source *pmem2_src);

#ifndef WIN32
int pmemset_source_from_file(struct pmemset_source **src, const char *file);

int pmemset_xsource_from_file(struct pmemset_source **src, const char *file,
		uint64_t flags);

int pmemset_source_from_temporary(struct pmemset_source **src, const char *dir);
#else
int pmemset_source_from_fileU(struct pmemset_source **src, const char *file);

int pmemset_xsource_from_fileU(struct pmemset_source **src, const char *file,
		uint64_t flags);

int pmemset_source_from_fileW(struct pmemset_source **src,
		const wchar_t *file);

int pmemset_xsource_from_fileW(struct pmemset_source **src, const wchar_t *file,
		unsigned flags);

int pmemset_source_from_temporaryU(struct pmemset_source **src,
		const char *dir);

int pmemset_source_from_temporaryW(struct pmemset_source **src,
		const wchar_t *dir);
#endif

int pmemset_source_delete(struct pmemset_source **src);

enum pmemset_part_state {
	/*
	 * The part state cannot be determined because of errors during
	 * retrieval of device information.
	 */
	PMEMSET_PART_STATE_INDETERMINATE = (1 << 0),

	/*
	 * The part is internally consistent and was closed cleanly.
	 * Application can assume that no custom recovery is needed.
	 */
	PMEMSET_PART_STATE_OK = (1 << 1),

	/*
	 * The part is internally consistent, but it is in use by the libpmemset
	 * library. It is an expected state when creating multiple mappings from
	 * the same source.
	 */
	PMEMSET_PART_STATE_OK_BUT_ALREADY_OPEN = (1 << 2),

	/* The part is internally consistent, but it was not closed cleanly. */
	PMEMSET_PART_STATE_OK_BUT_INTERRUPTED = (1 << 3),

	/*
	 * The part can contain invalid data as a result of hardware failure.
	 * Reading the part is unsafe. Application might need to perform
	 * consistency checking and custom recovery on user data.
	 */
	PMEMSET_PART_STATE_CORRUPTED = (1 << 4),
};

#define PMEMSET_SDS_INITIALIZE() { \
	.id = {0}, \
	.usc = 0, \
	.refcount = 0 \
}

#define PMEMSET_SDS_DEVICE_ID_LEN ((size_t)512ULL)

struct pmemset_sds {
	char id[PMEMSET_SDS_DEVICE_ID_LEN]; /* DIMM device id */
	uint64_t usc; /* unsafe shutdown count */
	int refcount;
};

int pmemset_source_set_sds(struct pmemset_source *src, struct pmemset_sds *sds,
		enum pmemset_part_state *state_ptr);

/* map, unmap and part operations */

int pmemset_map(struct pmemset_source *src,
		struct pmemset_map_config *map_cfg,
		struct pmemset_part_descriptor *desc);

struct pmemset_part_descriptor pmemset_descriptor_part_map(
		struct pmemset_part_map *pmap);

enum pmemset_coalescing {
	PMEMSET_COALESCING_NONE, /* don't try coalescing, default behavior */
	PMEMSET_COALESCING_OPPORTUNISTIC, /* try coalescing, dont fail */
	PMEMSET_COALESCING_FULL, /* coalesce, fail when impossible */
};

int pmemset_set_contiguous_part_coalescing(struct pmemset *set,
		enum pmemset_coalescing value);

int pmemset_part_map_by_address(struct pmemset *set,
		struct pmemset_part_map **pmap, void *addr);

void pmemset_first_part_map(struct pmemset *set,
		struct pmemset_part_map **pmap);

void pmemset_next_part_map(struct pmemset *set, struct pmemset_part_map *cur,
		struct pmemset_part_map **next);

void pmemset_part_map_drop(struct pmemset_part_map **pmap);

int pmemset_remove_part_map(struct pmemset *set,
		struct pmemset_part_map **part);

int pmemset_remove_range(struct pmemset *set, void *addr, size_t len);

/* pmemset mem flags */

#define PMEMSET_F_MEM_NODRAIN		(1U << 0)
#define PMEMSET_F_MEM_NONTEMPORAL	(1U << 1)
#define PMEMSET_F_MEM_TEMPORAL		(1U << 2)
#define PMEMSET_F_MEM_WC			(1U << 3)
#define PMEMSET_F_MEM_WB			(1U << 4)
#define PMEMSET_F_MEM_NOFLUSH		(1U << 5)

#define PMEMSET_F_MEM_VALID_FLAGS (PMEMSET_F_MEM_NODRAIN | \
		PMEMSET_F_MEM_NONTEMPORAL | \
		PMEMSET_F_MEM_TEMPORAL | \
		PMEMSET_F_MEM_WC | \
		PMEMSET_F_MEM_WB | \
		PMEMSET_F_MEM_NOFLUSH)

/* pmemset mem operations and persists */

void *pmemset_memmove(struct pmemset *set, void *pmemdest, void *src,
		size_t len, unsigned flags);

void *pmemset_memcpy(struct pmemset *set, void *pmemdest, void *src,
		size_t len, unsigned flags);

void *pmemset_memset(struct pmemset *set, void *pmemdest, int c, size_t len,
		unsigned flags);

int pmemset_persist(struct pmemset *set, void *ptr, size_t size);

int pmemset_flush(struct pmemset *set, void *ptr, size_t size);

int pmemset_deep_flush(struct pmemset *set, void *ptr, size_t size);

int pmemset_drain(struct pmemset *set);

/* error handling */

int pmemset_err_to_errno(int);

#ifndef _WIN32
const char *pmemset_errormsg(void);

void pmemset_perror(const char *format,
		...) __attribute__((__format__(__printf__, 1, 2)));
#else
const char *pmemset_errormsgU(void);

const wchar_t *pmemset_errormsgW(void);

void pmemset_perrorU(const char *format, ...);

void pmemset_perrorW(const wchar_t *format, ...);
#endif

#ifdef __cplusplus
}
#endif
#endif	/* libpmemset.h */
