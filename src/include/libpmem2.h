/*
 * Copyright 2019, Intel Corporation
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
 * libpmem2.h -- definitions of libpmem2 entry points (EXPERIMENTAL)
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
#define pmem2_errormsg pmem2_errormsgW
#else
#define pmem2_errormsg pmem2_errormsgU
#endif

#endif

#ifdef __cplusplus
extern "C" {
#endif

struct pmem2_config;
struct pmem2_map;
struct pmem2_badblock_iterator;
struct pmem2_badblock {
	size_t offset;
	size_t length;
};

enum pmem2_granularity {
	PMEM2_GRANULARITY_BYTE,
	PMEM2_GRANULARITY_CACHE_LINE,
	PMEM2_GRANULARITY_PAGE,
};

#define PMEM2_E_OK			0
#define PMEM2_E_EXTERNAL		1
#define PMEM2_E_INVALID_ARG		2
#define PMEM2_E_INVALID_HANDLE		3
#define PMEM2_E_NOMEM			4
#define PMEM2_E_MAP_RANGE		5
#define PMEM2_E_INV_FSIZE		6
#define PMEM2_E_UNKNOWN_FILETYPE	7
#define PMEM2_E_MAP_FAILED		8
#define PMEM2_E_NOSUPP			9

#define PMEM2_F_MEM_NODRAIN	(1U << 0)

#define PMEM2_F_MEM_NONTEMPORAL	(1U << 1)
#define PMEM2_F_MEM_TEMPORAL	(1U << 2)

#define PMEM2_F_MEM_WC	(1U << 3)
#define PMEM2_F_MEM_WB	(1U << 4)

#define PMEM2_F_MEM_NOFLUSH	(1U << 5)

#define PMEM2_F_MEM_VALID_FLAGS (PMEM2_F_MEM_NODRAIN | \
		PMEM2_F_MEM_NONTEMPORAL | \
		PMEM2_F_MEM_TEMPORAL | \
		PMEM2_F_MEM_WC | \
		PMEM2_F_MEM_WB | \
		PMEM2_F_MEM_NOFLUSH)

#define PMEM2_ADDRESS_ANY		0 /* default */
#define PMEM2_ADDRESS_FIXED_REPLACE	1
#define PMEM2_ADDRESS_FIXED_NOREPLACE	2

#define PMEM2_SHARED	0 /* default */
#define PMEM2_PRIVATE	1

#define PMEM2_PROT_FROM_FD	0 /* default */
#define PMEM2_PROT_EXEC		(1U<<29)
#define PMEM2_PROT_READ		(1U<<30)
#define PMEM2_PROT_WRITE	(1U<<31)

typedef void (*pmem2_persist_fn)(void *ptr, size_t size);
typedef void (*pmem2_flush_fn)(void *ptr, size_t size);
typedef void (*pmem2_drain_fn)(void);

static inline int pmem2_config_new(struct pmem2_config **cfg)
{
	return PMEM2_E_NOSUPP;
}

static inline int pmem2_config_set_fd(struct pmem2_config *cfg, int fd)
{
	return PMEM2_E_NOSUPP;
}

static inline int pmem2_config_delete(struct pmem2_config *cfg)
{
	return PMEM2_E_NOSUPP;
}

static inline pmem2_persist_fn *pmem2_get_persist_fn(struct pmem2_map *map)
{
	return NULL;
}
static inline pmem2_flush_fn *pmem2_get_flush_fn(struct pmem2_map *map)
{
	return NULL;
}
static inline pmem2_drain_fn *pmem2_get_drain_fn(struct pmem2_map *map)
{
	return NULL;
}

static inline int pmem2_config_set_required_store_granularity(
		struct pmem2_config *cfg, enum pmem2_granularity g)
{
	return PMEM2_E_NOSUPP;
}

static inline enum pmem2_granularity pmem2_map_get_store_granularity(
		struct pmem2_map *map)
{
	return PMEM2_E_NOSUPP;
}

static inline int pmem2_config_set_offset(struct pmem2_config *cfg,
		size_t offset)
{
	return PMEM2_E_NOSUPP;
}

static inline int pmem2_config_set_length(struct pmem2_config *cfg,
		size_t length)
{
	return PMEM2_E_NOSUPP;
}

typedef void (*pmem2_memmove_fn)(void *pmemdest, const void *src, size_t len,
		unsigned flags);
typedef void (*pmem2_memcpy_fn)(void *pmemdest, const void *src, size_t len,
		unsigned flags);
typedef void (*pmem2_memset_fn)(void *pmemdest, int c, size_t len,
		unsigned flags);

static inline pmem2_memmove_fn *pmem2_get_memmove_fn(struct pmem2_map *map)
{
	return NULL;
}

static inline pmem2_memcpy_fn *pmem2_get_memcpy_fn(struct pmem2_map *map)
{
	return NULL;
}

static inline pmem2_memset_fn *pmem2_get_memset_fn(struct pmem2_map *map)
{
	return NULL;
}

static inline struct pmem2_config *pmem2_get_config(struct pmem2_map *map)
{
	return NULL;
}

static inline int pmem2_config_set_address(struct pmem2_config *cfg,
		unsigned type, void *addr)
{
	return PMEM2_E_NOSUPP;
}

static inline int pmem2_get_device_id(const struct pmem2_config *cfg, char *id,
		size_t *len)
{
	return PMEM2_E_NOSUPP;
}

static inline int pmem2_get_device_usc(const struct pmem2_config *cfg,
		uint64_t *usc)
{
	return PMEM2_E_NOSUPP;
}

static inline int pmem2_badblock_iterator_new(const struct pmem2_config *cfg,
		struct pmem2_badblock_iterator **pbb)
{
	return PMEM2_E_NOSUPP;
}

static inline int pmem2_badblock_next(struct pmem2_badblock_iterator *pbb,
		struct pmem2_badblock *bb)
{
	return PMEM2_E_NOSUPP;
}

static inline void pmem2_badblock_iterator_delete(
		struct pmem2_badblock_iterator **pbb)
{
	;
}

static inline int pmem2_badblock_clear(const struct pmem2_config *cfg,
		const struct pmem2_badblock *bb)
{
	return PMEM2_E_NOSUPP;
}

static inline int pmem2_config_set_sharing(struct pmem2_config *cfg,
		unsigned type)
{
	return PMEM2_E_NOSUPP;
}

static inline int pmem2_config_set_protection(struct pmem2_config *cfg,
		unsigned flag)
{
	return PMEM2_E_NOSUPP;
}

static inline int pmem2_config_use_anonymous_mapping(struct pmem2_config *cfg,
		unsigned on)
{
	return PMEM2_E_NOSUPP;
}

#ifndef _WIN32
const char *pmem2_errormsg(void);
#else
const char *pmem2_errormsgU(void);
const wchar_t *pmem2_errormsgW(void);
#endif

#ifdef __cplusplus
}
#endif
#endif	/* libpmem2.h */
