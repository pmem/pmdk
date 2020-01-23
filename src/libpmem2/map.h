/*
 * Copyright 2019-2020, Intel Corporation
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
 * map.h -- internal definitions for libpmem2
 */
#ifndef PMEM2_MAP_H
#define PMEM2_MAP_H

#include <stddef.h>
#include <stdbool.h>
#include "libpmem2.h"

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct pmem2_map {
	void *addr; /* base address */
	size_t reserved_length; /* length of the mapping reservation */
	size_t content_length; /* length of the mapped content */
	/* effective persistence granularity */
	enum pmem2_granularity effective_granularity;

	pmem2_persist_fn persist_fn;
	pmem2_flush_fn flush_fn;
	pmem2_drain_fn drain_fn;

	pmem2_memmove_fn memmove_fn;
	pmem2_memcpy_fn memcpy_fn;
	pmem2_memset_fn memset_fn;

#ifdef _WIN32
	HANDLE handle;
#endif
};

enum pmem2_granularity get_min_granularity(bool eADR, bool is_pmem);
struct pmem2_map *pmem2_map_find(const void *addr, size_t len);
int pmem2_register_mapping(struct pmem2_map *map);
int pmem2_unregister_mapping(struct pmem2_map *map);
void pmem2_map_init(void);
void pmem2_map_fini(void);

int pmem2_validate_offset(const struct pmem2_config *cfg, size_t *offset);

#ifdef __cplusplus
}
#endif

#endif /* map.h */
