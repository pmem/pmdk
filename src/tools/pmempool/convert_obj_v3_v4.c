/*
 * Copyright 2017, Intel Corporation
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
 * convert_obj_v3_v4.c -- pmempool convert command source file
 */

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <endian.h>
#include <stddef.h>
#include "convert.h"

#define PMEMOBJ_MAX_LAYOUT ((size_t)1024)
#define OBJ_DSC_P_SIZE		2048
#define OBJ_DSC_P_UNUSED	(OBJ_DSC_P_SIZE - PMEMOBJ_MAX_LAYOUT - 40)

struct pool_hdr {
	char data[4096];
};

struct pmem_ops {
	void *persist;
	void *flush;
	void *drain;
	void *memcpy_persist;
	void *memset_persist;

	void *base;
	size_t pool_size;

	struct remote_ops {
		void *read;

		void *ctx;
		uintptr_t base;
	} remote;
};

struct palloc_heap {
	struct pmem_ops p_ops;
	struct heap_layout *layout;
	struct heap_rt *rt;
	uint64_t size;
	uint64_t run_id;

	void *base;
};

struct lane_descriptor {
	unsigned runtime_nlanes;
	unsigned next_lane_idx;
	uint64_t *lane_locks;
	struct lane *lane;
};

#define _POBJ_CL_SIZE 64
typedef union {
	long long align;
	char padding[_POBJ_CL_SIZE];
} PMEMmutex;

struct pmemobjpool {
	struct pool_hdr hdr;	/* memory pool header */

	/* persistent part of PMEMOBJ pool descriptor (2kB) */
	char layout[PMEMOBJ_MAX_LAYOUT];
	uint64_t lanes_offset;
	uint64_t nlanes;
	uint64_t heap_offset;
	uint64_t heap_size;
	unsigned char unused[OBJ_DSC_P_UNUSED]; /* must be zero */
	uint64_t checksum;	/* checksum of above fields */

	uint64_t root_offset;

	/* unique runID for this program run - persistent but not checksummed */
	uint64_t run_id;

	uint64_t root_size;

	uint64_t conversion_flags;

	char pmem_reserved[512]; /* must be zeroed */

	/* some run-time state, allocated out of memory pool... */
	void *addr;		/* mapped region */
	size_t size;		/* size of mapped region */
	int is_pmem;		/* true if pool is PMEM */
	int rdonly;		/* true if pool is opened read-only */
	struct palloc_heap heap;
	struct lane_descriptor lanes_desc;
	uint64_t uuid_lo;
	int is_dev_dax;		/* true if mapped on device dax */

	void *ctl;
	void *tx_postcommit_tasks;

	void *set;	/* pool set info */
	void *replica;	/* next replica */
	void *redo;

	/* per-replica functions: pmem or non-pmem */
	void *persist_local;	/* persist function */
	void *flush_local;	/* flush function */
	void *drain_local;	/* drain function */
	void *memcpy_persist_local; /* persistent memcpy function */
	void *memset_persist_local; /* persistent memset function */

	/* for 'master' replica: with or without data replication */
	struct pmem_ops p_ops;

	PMEMmutex rootlock;	/* root object lock */
	int is_master_replica;
	int has_remote_replicas;

	/* remote replica section */
	void *rpp;	/* RPMEMpool opaque handle if it is a remote replica */
	uintptr_t remote_base;	/* beginning of the pool's descriptor */
	char *node_addr;	/* address of a remote node */
	char *pool_desc;	/* descriptor of a poolset */

	void *persist_remote; /* remote persist function */

	int vg_boot;
	int tx_debug_skip_expensive_checks;

	void *tx_params;

	/* padding to align size of this structure to page boundary */
	/* sizeof(unused2) == 8192 - offsetof(struct pmemobjpool, unused2) */
	char unused2[1028];
};

struct allocation_header_legacy {
	uint8_t unused[8];
	uint64_t size;
	uint8_t unused2[32];
	uint64_t root_size;
	uint64_t type_num;
};

static void
obj_root_restore_size(struct pmemobjpool *pop)
{
#define LEGACY_INTERNAL_OBJECT_MASK ((1ULL) << 63)
	if (pop->root_offset == 0)
		return;

	uint64_t off = pop->root_offset;
	off -= sizeof(struct allocation_header_legacy);
	struct allocation_header_legacy *hdr =
		(struct allocation_header_legacy *)((uintptr_t)pop + off);

	pop->root_size = hdr->root_size & ~LEGACY_INTERNAL_OBJECT_MASK;
}

#define CONVERSION_FLAG_OLD_SET_CACHE ((1ULL) << 0)

int
convert_v3_v4(void *psf, void *addr)
{
	struct pmemobjpool *pop = addr;

	assert(sizeof(struct pmemobjpool) == 8192);

	pop->conversion_flags = CONVERSION_FLAG_OLD_SET_CACHE;

	/* zero out the pmem reserved part of the header */
	memset(pop->pmem_reserved, 0, sizeof(pop->pmem_reserved));

	obj_root_restore_size(pop);

	pmempool_convert_persist(psf, pop, sizeof(*pop));

	return 0;
}
