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
#define pmemset_config_set_layout_name pmemset_config_set_layout_nameW
#define pmemset_header_init pmemset_header_initW
#define pmemset_source_from_file pmemset_source_from_fileW
#define pmemset_source_from_temporary pmemset_source_from_temporaryW
#define pmemset_errormsg pmemset_errormsgW
#define pmemset_perror pmemset_perrorW
#else
#define pmemset_config_set_layout_name pmemset_config_set_layout_nameU
#define pmemset_header_init pmemset_header_initU
#define pmemset_source_from_file pmemset_source_from_fileU
#define pmemset_source_from_temporary pmemset_source_from_temporaryU
#define pmemset_errormsg pmemset_errormsgU
#define pmemset_perror pmemset_perrorU
#endif

#endif

#ifdef __cplusplus
extern "C" {
#endif

#define PMEMSET_E_UNKNOWN				(-200000)
#define PMEMSET_E_NOSUPP				(-200001)
#define PMEMSET_E_INVALID_PMEM2_SOURCE			(-200002)
#define PMEMSET_E_INVALID_FILE_PATH			(-200003)
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

/* pmemset setup */

enum pmemset_coalescing {
	PMEMSET_COALESCING_NONE, /* don't try coalescing, default behavior */
	PMEMSET_COALESCING_OPPORTUNISTIC, /* try coalescing, dont fail */
	PMEMSET_COALESCING_FULL, /* coalesce, fail when impossible */
};

struct pmemset;
struct pmemset_config;
struct pmemset_header;
struct pmemset_part;
struct pmemset_part_map;

struct pmemset_part_descriptor {
	void *addr;
	size_t size;
};

struct pmemset_extras {
	const struct pmemset_header *header_in;
	struct pmemset_header *header_out;
	const struct pmemset_part_shutdown_state_data *sds_in;
	struct pmemset_part_shutdown_state_data *sds_out;
	enum pmemset_part_state *state;
};

int pmemset_new(struct pmemset **set, struct pmemset_config *cfg);

int pmemset_delete(struct pmemset **set);

#ifndef WIN32
int pmemset_header_init(struct pmemset_header *header, const char *layout,
		int major, int minor);
#else
int pmemset_header_initU(struct pmemset_header *header, const char *layout,
		int major, int minor);

int pmemset_header_initW(struct pmemset_header *header, const wchar_t *layout,
		int major, int minor);
#endif

int pmemset_remove_part_map(struct pmemset *set,
		struct pmemset_part_map **part);

struct pmemset_part_descriptor pmemset_descriptor_part_map(
		struct pmemset_part_map *pmap);

void pmemset_first_part_map(struct pmemset *set,
		struct pmemset_part_map **pmap);

void pmemset_next_part_map(struct pmemset *set, struct pmemset_part_map *cur,
		struct pmemset_part_map **next);

int pmemset_remove_range(struct pmemset *set, void *addr, size_t len);

int pmemset_set_contiguous_part_coalescing(struct pmemset *set,
		enum pmemset_coalescing value);

int pmemset_persist(struct pmemset *set, const void *ptr, size_t size);

int pmemset_flush(struct pmemset *set, const void *ptr, size_t size);

int pmemset_drain(struct pmemset *set);

int pmemset_get_store_granularity(struct pmemset *set,
		enum pmem2_granularity *g);

#define PMEMSET_F_MEM_NODRAIN		(1U << 0)
#define PMEMSET_F_MEM_NONTEMPORAL	(1U << 1)
#define PMEMSET_F_MEM_TEMPORAL		(1U << 2)
#define PMEMSET_F_MEM_WC		(1U << 3)
#define PMEMSET_F_MEM_WB		(1U << 4)
#define PMEMSET_F_MEM_NOFLUSH		(1U << 5)

#define PMEMSET_F_MEM_VALID_FLAGS (PMEMSET_F_MEM_NODRAIN | \
		PMEMSET_F_MEM_NONTEMPORAL | \
		PMEMSET_F_MEM_TEMPORAL | \
		PMEMSET_F_MEM_WC | \
		PMEMSET_F_MEM_WB | \
		PMEMSET_F_MEM_NOFLUSH)

void *pmemset_memmove(struct pmemset *set, void *pmemdest, const void *src,
		size_t len, unsigned flags);

void *pmemset_memcpy(struct pmemset *set, void *pmemdest, const void *src,
		size_t len, unsigned flags);

void *pmemset_memset(struct pmemset *set, void *pmemdest, int c, size_t len,
		unsigned flags);

int pmemset_deep_flush(struct pmemset *set, void *ptr, size_t size);

/* event setup */

struct pmemset_event_context;

typedef int pmemset_event_callback(struct pmemset *set,
		struct pmemset_event_context *context, void *arg);

/* config setup */

int pmemset_config_new(struct pmemset_config **cfg);

int pmemset_config_delete(struct pmemset_config **cfg);

int pmemset_config_set_event_callback(struct pmemset_config *cfg,
		pmemset_event_callback *callback, void *arg);

int pmemset_config_set_reservation(struct pmemset_config *cfg,
		struct pmem2_vm_reservation *rsv);

int pmemset_config_set_required_store_granularity(struct pmemset_config *cfg,
		enum pmem2_granularity g);

#ifndef _WIN32
int pmemset_config_set_layout_name(struct pmemset_config *cfg,
		const char *layout);
#else
int pmemset_config_set_layout_nameU(struct pmemset_config *cfg,
		const char *layout);

int pmemset_config_set_layout_nameW(struct pmemset_config *cfg,
		const wchar_t *layout);
#endif

int pmemset_config_set_version(struct pmemset_config *cfg,
		int major, int minor);

/* source setup */

struct pmem2_source;
struct pmemset_source;

int pmemset_source_from_pmem2(struct pmemset_source **src,
	struct pmem2_source *pmem2_src);

#define PMEMSET_SOURCE_FILE_CREATE_ALWAYS		(1U << 0)
#define PMEMSET_SOURCE_FILE_CREATE_IF_NEEDED		(1U << 1)
#define PMEMSET_SOURCE_FILE_CREATE_ALL (PMEMSET_SOURCE_FILE_CREATE_ALWAYS | \
		PMEMSET_SOURCE_FILE_CREATE_IF_NEEDED)

#ifndef WIN32
int pmemset_source_from_file(struct pmemset_source **src, const char *file,
				unsigned flags);

int pmemset_source_from_temporary(struct pmemset_source **src, const char *dir,
		size_t len);
#else
int pmemset_source_from_fileU(struct pmemset_source **src, const char *file,
				unsigned flags);

int pmemset_source_from_fileW(struct pmemset_source **src, const wchar_t *file,
				unsigned flags);

int pmemset_source_from_temporaryU(struct pmemset_source **src,
		const char *dir, size_t len);

int pmemset_source_from_temporaryW(struct pmemset_source **src,
		const wchar_t *dir, size_t len);
#endif

int pmemset_source_delete(struct pmemset_source **src);

/* part setup */

struct pmemset_part;
struct pmemset_part_map;
struct pmemset_part_shutdown_state_data;

enum pmemset_part_state {
	/*
	 * The pool state cannot be determined because of errors during
	 * retrieval of device information.
	 */
	PMEMSET_PART_STATE_INDETERMINATE,

	/*
	 * The pool is internally consistent and was closed cleanly.
	 * Application can assume that no custom recovery is needed.
	 */
	PMEMSET_PART_STATE_OK,

	/*
	 * The pool is internally consistent, but it was not closed cleanly.
	 * Application must perform consistency checking and custom recovery
	 * on user data.
	 */
	PMEMSET_PART_STATE_OK_BUT_INTERRUPTED,

	/*
	 * The pool can contain invalid data as a result of hardware failure.
	 * Reading the pool is unsafe.
	 */
	PMEMSET_PART_STATE_CORRUPTED,
};

int pmemset_part_new(struct pmemset_part **part, struct pmemset *set,
		struct pmemset_source *src, size_t offset, size_t length);
int pmemset_part_delete(struct pmemset_part **part);
int pmemset_part_pread_mcsafe(struct pmemset_part_descriptor *part,
		void *dst, size_t size, size_t offset);

int pmemset_part_pwrite_mcsafe(struct pmemset_part_descriptor *part,
		void *dst, size_t size, size_t offset);

int pmemset_part_map(struct pmemset_part **part, struct pmemset_extras *extra,
		struct pmemset_part_descriptor *desc);

void pmemset_part_map_drop(struct pmemset_part_map **pmap);

int pmemset_part_map_by_address(struct pmemset *set,
		struct pmemset_part_map **pmap, void *addr);

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
