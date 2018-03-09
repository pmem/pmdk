/*
 * Copyright 2016-2018, Intel Corporation
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
 * cto.h -- internal definitions for libpmemcto module
 */
#ifndef LIBPMEMCTO_CTO_H
#define LIBPMEMCTO_CTO_H 1

#include "os_thread.h"
#include "util.h"
#include "pool_hdr.h"

#define PMEMCTO_LOG_PREFIX "libpmemcto"
#define PMEMCTO_LOG_LEVEL_VAR "PMEMCTO_LOG_LEVEL"
#define PMEMCTO_LOG_FILE_VAR "PMEMCTO_LOG_FILE"

/* attributes of the cto memory pool format for the pool header */
#define CTO_HDR_SIG "PMEMCTO"	/* must be 8 bytes including '\0' */
#define CTO_FORMAT_MAJOR 1

#define CTO_FORMAT_COMPAT_DEFAULT 0x0000
#define CTO_FORMAT_INCOMPAT_DEFAULT 0x0000
#define CTO_FORMAT_RO_COMPAT_DEFAULT 0x0000

#define CTO_FORMAT_COMPAT_CHECK 0x0000
#define CTO_FORMAT_INCOMPAT_CHECK POOL_FEAT_ALL
#define CTO_FORMAT_RO_COMPAT_CHECK 0x0000

/* size of the persistent part of PMEMOBJ pool descriptor (2kB) */
#define CTO_DSC_P_SIZE		2048
/* size of unused part of the persistent part of PMEMOBJ pool descriptor */
#define CTO_DSC_P_UNUSED	(CTO_DSC_P_SIZE - PMEMCTO_MAX_LAYOUT - 28)

/*
 * XXX: We don't care about portable data types, as the pool may only be open
 * on the same platform.
 * Assuming the shutdown state / consistent flag is updated in a fail-safe
 * manner, there is no need to checksum the persistent part of the descriptor.
 */
struct pmemcto {
	struct pool_hdr hdr;	/* memory pool header */

	/* persistent part of PMEMCTO pool descriptor (2kB) */
	char layout[PMEMCTO_MAX_LAYOUT];
	uint64_t addr;		/* mapped region */
	uint64_t size;		/* size of mapped region */
	uint64_t root;		/* root pointer */

	uint8_t consistent;	/* successfully flushed before exit */
	unsigned char unused[CTO_DSC_P_UNUSED]; /* must be zero */

	/* some run-time state, allocated out of memory pool... */
	struct pool_set *set;	/* pool set info */
	int is_pmem;		/* true if pool is PMEM */
	int rdonly;		/* true if pool is opened read-only */
	int is_dev_dax;		/* true if mapped on device dax */
};

/* data area starts at this alignment after the struct pmemcto above */
#define CTO_FORMAT_DATA_ALIGN ((uintptr_t)4096)

#define CTO_DSC_SIZE (sizeof(struct pmemcto) - sizeof(struct pool_hdr))
#define CTO_DSC_SIZE_ALIGNED\
	roundup(sizeof(struct pmemcto), CTO_FORMAT_DATA_ALIGN)

void cto_init(void);
void cto_fini(void);

#ifdef _WIN32
/*
 * On Linux we have separate jemalloc builds for libvmem, libvmmalloc
 * and libpmemcto, with different function name prefixes.  This is to avoid
 * symbol collisions in case of static linking of those libraries.
 * On Windows we don't provide statically linked libraries, so there is
 * no need to have separate jemalloc builds.  However, since libpmemcto
 * links to jemalloc symbols with "je_cto" prefix, we have to do renaming
 * here (unless there is a better solution).
 */
#define je_cto_pool_create je_vmem_pool_create
#define je_cto_pool_delete je_vmem_pool_delete
#define je_cto_pool_malloc je_vmem_pool_malloc
#define je_cto_pool_calloc je_vmem_pool_calloc
#define je_cto_pool_ralloc je_vmem_pool_ralloc
#define je_cto_pool_aligned_alloc je_vmem_pool_aligned_alloc
#define je_cto_pool_free je_vmem_pool_free
#define je_cto_pool_malloc_usable_size je_vmem_pool_malloc_usable_size
#define je_cto_pool_malloc_stats_print je_vmem_pool_malloc_stats_print
#define je_cto_pool_extend je_vmem_pool_extend
#define je_cto_pool_set_alloc_funcs je_vmem_pool_set_alloc_funcs
#define je_cto_pool_check je_vmem_pool_check
#define je_cto_malloc_message je_vmem_malloc_message
#endif

#endif /* LIBPMEMCTO_CTO_H */
