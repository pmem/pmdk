/*
 * Copyright 2014-2019, Intel Corporation
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
 * pmem.h -- internal definitions for libpmem
 */
#ifndef PMEM_H
#define PMEM_H

#include <stddef.h>
#include "alloc.h"
#include "fault_injection.h"
#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PMEM_LOG_PREFIX "libpmem"
#define PMEM_LOG_LEVEL_VAR "PMEM_LOG_LEVEL"
#define PMEM_LOG_FILE_VAR "PMEM_LOG_FILE"

typedef int (*is_pmem_func)(const void *addr, size_t len);

void pmem_init(void);
void pmem_os_init(is_pmem_func *func);

int is_pmem_detect(const void *addr, size_t len);
void *pmem_map_register(int fd, size_t len, const char *path, int is_dev_dax);

#if FAULT_INJECTION
void
pmem_inject_fault_at(enum pmem_allocation_type type, int nth,
						const char *at);

int
pmem_fault_injection_enabled(void);
#else
static inline void
pmem_inject_fault_at(enum pmem_allocation_type type, int nth,
						const char *at)
{
	abort();
}

static inline int
pmem_fault_injection_enabled(void)
{
	return 0;
}
#endif

#ifdef __cplusplus
}
#endif

#endif
