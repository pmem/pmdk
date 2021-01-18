/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2019-2021, Intel Corporation */

/*
 * libpmem2.h -- definitions of libpmem2 entry points
 *
 * This library provides support for programming with persistent memory (pmem).
 *
 * libpmem2 provides support for using raw pmem directly.
 *
 * See libpmem2(7) for details.
 */

#ifndef LIBPMEM2_H
#define LIBPMEM2_H 1

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#include <pmemcompat.h>

#ifndef PMDK_UTF8_API
#define pmem2_source_device_id pmem2_source_device_idW
#define pmem2_errormsg pmem2_errormsgW
#define pmem2_perror pmem2_perrorW
#else
#define pmem2_source_device_id pmem2_source_device_idU
#define pmem2_errormsg pmem2_errormsgU
#define pmem2_perror pmem2_perrorU
#endif

#endif

#ifdef __cplusplus
extern "C" {
#endif

#define PMEM2_E_UNKNOWN				(-100000)
#define PMEM2_E_NOSUPP				(-100001)
#define PMEM2_E_FILE_HANDLE_NOT_SET		(-100003)
#define PMEM2_E_INVALID_FILE_HANDLE		(-100004)
#define PMEM2_E_INVALID_FILE_TYPE		(-100005)
#define PMEM2_E_MAP_RANGE			(-100006)
#define PMEM2_E_MAPPING_EXISTS			(-100007)
#define PMEM2_E_GRANULARITY_NOT_SET		(-100008)
#define PMEM2_E_GRANULARITY_NOT_SUPPORTED	(-100009)
#define PMEM2_E_OFFSET_OUT_OF_RANGE		(-100010)
#define PMEM2_E_OFFSET_UNALIGNED		(-100011)
#define PMEM2_E_INVALID_ALIGNMENT_FORMAT	(-100012)
#define PMEM2_E_INVALID_ALIGNMENT_VALUE		(-100013)
#define PMEM2_E_INVALID_SIZE_FORMAT		(-100014)
#define PMEM2_E_LENGTH_UNALIGNED		(-100015)
#define PMEM2_E_MAPPING_NOT_FOUND		(-100016)
#define PMEM2_E_BUFFER_TOO_SMALL		(-100017)
#define PMEM2_E_SOURCE_EMPTY			(-100018)
#define PMEM2_E_INVALID_SHARING_VALUE		(-100019)
#define PMEM2_E_SRC_DEVDAX_PRIVATE		(-100020)
#define PMEM2_E_INVALID_ADDRESS_REQUEST_TYPE	(-100021)
#define PMEM2_E_ADDRESS_UNALIGNED		(-100022)
#define PMEM2_E_ADDRESS_NULL			(-100023)
#define PMEM2_E_DEEP_FLUSH_RANGE		(-100024)
#define PMEM2_E_INVALID_REGION_FORMAT		(-100025)
#define PMEM2_E_DAX_REGION_NOT_FOUND		(-100026)
#define PMEM2_E_INVALID_DEV_FORMAT		(-100027)
#define PMEM2_E_CANNOT_READ_BOUNDS		(-100028)
#define PMEM2_E_NO_BAD_BLOCK_FOUND		(-100029)
#define PMEM2_E_LENGTH_OUT_OF_RANGE		(-100030)
#define PMEM2_E_INVALID_PROT_FLAG		(-100031)
#define PMEM2_E_NO_ACCESS			(-100032)
#define PMEM2_E_VM_RESERVATION_NOT_EMPTY	(-100033)
#define PMEM2_E_MAP_EXISTS			(-100034)
#define PMEM2_E_FILE_DESCRIPTOR_NOT_SET		(-100035)

/* source setup */

struct pmem2_source;

int pmem2_source_from_fd(struct pmem2_source **src, int fd);
int pmem2_source_from_anon(struct pmem2_source **src, size_t size);
#ifdef _WIN32
int pmem2_source_from_handle(struct pmem2_source **src, HANDLE handle);
int pmem2_source_get_handle(const struct pmem2_source *src, HANDLE *h);
#else
int pmem2_source_get_fd(const struct pmem2_source *src, int *fd);
#endif

int pmem2_source_size(const struct pmem2_source *src, size_t *size);

int pmem2_source_alignment(const struct pmem2_source *src,
		size_t *alignment);

int pmem2_source_delete(struct pmem2_source **src);

/* vm reservation setup */

struct pmem2_vm_reservation;

void *pmem2_vm_reservation_get_address(struct pmem2_vm_reservation *rsv);

size_t pmem2_vm_reservation_get_size(struct pmem2_vm_reservation *rsv);

int pmem2_vm_reservation_new(struct pmem2_vm_reservation **rsv_ptr,
		void *addr, size_t size);

int pmem2_vm_reservation_delete(struct pmem2_vm_reservation **rsv_ptr);

int pmem2_vm_reservation_extend(struct pmem2_vm_reservation *rsv, size_t size);

int pmem2_vm_reservation_shrink(struct pmem2_vm_reservation *rsv, size_t offset,
		size_t size);

/* config setup */

struct pmem2_config;

int pmem2_config_new(struct pmem2_config **cfg);

int pmem2_config_delete(struct pmem2_config **cfg);

enum pmem2_granularity {
	PMEM2_GRANULARITY_BYTE,
	PMEM2_GRANULARITY_CACHE_LINE,
	PMEM2_GRANULARITY_PAGE,
};

int pmem2_config_set_required_store_granularity(struct pmem2_config *cfg,
	enum pmem2_granularity g);

int pmem2_config_set_offset(struct pmem2_config *cfg, size_t offset);

int pmem2_config_set_length(struct pmem2_config *cfg, size_t length);

enum pmem2_sharing_type {
	PMEM2_SHARED,
	PMEM2_PRIVATE,
};

int pmem2_config_set_sharing(struct pmem2_config *cfg,
				enum pmem2_sharing_type type);

#define PMEM2_PROT_EXEC	(1U << 29)
#define PMEM2_PROT_READ	(1U << 30)
#define PMEM2_PROT_WRITE	(1U << 31)
#define PMEM2_PROT_NONE	0

int pmem2_config_set_protection(struct pmem2_config *cfg,
				unsigned prot);

int pmem2_config_set_vm_reservation(struct pmem2_config *cfg,
		struct pmem2_vm_reservation *rsv, size_t offset);

/* mapping */

struct pmem2_map;
int pmem2_map_from_existing(struct pmem2_map **map,
	const struct pmem2_source *src, void *addr, size_t len,
	enum pmem2_granularity gran);

int pmem2_map_new(struct pmem2_map **map_ptr, const struct pmem2_config *cfg,
		const struct pmem2_source *src);

int pmem2_map_delete(struct pmem2_map **map_ptr);

void *pmem2_map_get_address(struct pmem2_map *map);

size_t pmem2_map_get_size(struct pmem2_map *map);

enum pmem2_granularity pmem2_map_get_store_granularity(struct pmem2_map *map);

/* flushing */

typedef void (*pmem2_persist_fn)(const void *ptr, size_t size);

typedef void (*pmem2_flush_fn)(const void *ptr, size_t size);

typedef void (*pmem2_drain_fn)(void);

pmem2_persist_fn pmem2_get_persist_fn(struct pmem2_map *map);

pmem2_flush_fn pmem2_get_flush_fn(struct pmem2_map *map);

pmem2_drain_fn pmem2_get_drain_fn(struct pmem2_map *map);

#define PMEM2_F_MEM_NODRAIN	(1U << 0)

#define PMEM2_F_MEM_NONTEMPORAL	(1U << 1)
#define PMEM2_F_MEM_TEMPORAL	(1U << 2)

#define PMEM2_F_MEM_WC		(1U << 3)
#define PMEM2_F_MEM_WB		(1U << 4)

#define PMEM2_F_MEM_NOFLUSH	(1U << 5)

#define PMEM2_F_MEM_VALID_FLAGS (PMEM2_F_MEM_NODRAIN | \
		PMEM2_F_MEM_NONTEMPORAL | \
		PMEM2_F_MEM_TEMPORAL | \
		PMEM2_F_MEM_WC | \
		PMEM2_F_MEM_WB | \
		PMEM2_F_MEM_NOFLUSH)

typedef void *(*pmem2_memmove_fn)(void *pmemdest, const void *src, size_t len,
		unsigned flags);

typedef void *(*pmem2_memcpy_fn)(void *pmemdest, const void *src, size_t len,
		unsigned flags);

typedef void *(*pmem2_memset_fn)(void *pmemdest, int c, size_t len,
		unsigned flags);

pmem2_memmove_fn pmem2_get_memmove_fn(struct pmem2_map *map);

pmem2_memcpy_fn pmem2_get_memcpy_fn(struct pmem2_map *map);

pmem2_memset_fn pmem2_get_memset_fn(struct pmem2_map *map);

/* RAS */

int pmem2_deep_flush(struct pmem2_map *map, void *ptr, size_t size);

#ifndef _WIN32
int pmem2_source_device_id(const struct pmem2_source *src,
	char *id, size_t *len);
#else
int pmem2_source_device_idW(const struct pmem2_source *src,
	wchar_t *id, size_t *len);

int pmem2_source_device_idU(const struct pmem2_source *src,
	char *id, size_t *len);
#endif

int pmem2_source_device_usc(const struct pmem2_source *src, uint64_t *usc);

int pmem2_source_numa_node(const struct pmem2_source *src, int *numa_node);

struct pmem2_badblock_context;

struct pmem2_badblock {
	size_t offset;
	size_t length;
};

int pmem2_badblock_context_new(struct pmem2_badblock_context **bbctx,
		const struct pmem2_source *src);

int pmem2_badblock_next(struct pmem2_badblock_context *bbctx,
		struct pmem2_badblock *bb);

void pmem2_badblock_context_delete(
		struct pmem2_badblock_context **bbctx);

int pmem2_badblock_clear(struct pmem2_badblock_context *bbctx,
		const struct pmem2_badblock *bb);

/* error handling */

#ifndef _WIN32
const char *pmem2_errormsg(void);
#else
const char *pmem2_errormsgU(void);

const wchar_t *pmem2_errormsgW(void);
#endif

int pmem2_err_to_errno(int);

#ifndef _WIN32
void pmem2_perror(const char *format,
		...) __attribute__((__format__(__printf__, 1, 2)));
#else
void pmem2_perrorU(const char *format, ...);

void pmem2_perrorW(const wchar_t *format, ...);
#endif

#ifdef __cplusplus
}
#endif
#endif	/* libpmem2.h */
