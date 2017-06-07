/*
 * Copyright 2014-2017, Intel Corporation
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
 * obj.h -- internal definitions for obj module
 */

#ifndef LIBPMEMOBJ_OBJ_H
#define LIBPMEMOBJ_OBJ_H 1

#include <stddef.h>
#include <stdint.h>

#include "lane.h"
#include "pool_hdr.h"
#include "pmalloc.h"
#include "redo.h"
#include "ctl.h"
#include "ringbuf.h"

#define PMEMOBJ_LOG_PREFIX "libpmemobj"
#define PMEMOBJ_LOG_LEVEL_VAR "PMEMOBJ_LOG_LEVEL"
#define PMEMOBJ_LOG_FILE_VAR "PMEMOBJ_LOG_FILE"

/* attributes of the obj memory pool format for the pool header */
#define OBJ_HDR_SIG "PMEMOBJ"	/* must be 8 bytes including '\0' */
#define OBJ_FORMAT_MAJOR 3
#define OBJ_FORMAT_COMPAT 0x0000
#define OBJ_FORMAT_INCOMPAT 0x0000
#define OBJ_FORMAT_RO_COMPAT 0x0000

/* size of the persistent part of PMEMOBJ pool descriptor (2kB) */
#define OBJ_DSC_P_SIZE		2048
/* size of unused part of the persistent part of PMEMOBJ pool descriptor */
#define OBJ_DSC_P_UNUSED	(OBJ_DSC_P_SIZE - PMEMOBJ_MAX_LAYOUT - 40)

#define OBJ_LANES_OFFSET	8192	/* lanes offset (8kB) */
#define OBJ_NLANES		1024	/* number of lanes */

#define OBJ_OFF_TO_PTR(pop, off) ((void *)((uintptr_t)(pop) + (off)))
#define OBJ_PTR_TO_OFF(pop, ptr) ((uintptr_t)(ptr) - (uintptr_t)(pop))
#define OBJ_OID_IS_NULL(oid)	((oid).off == 0)
#define OBJ_LIST_EMPTY(head)	OBJ_OID_IS_NULL((head)->pe_first)
#define OBJ_OFF_FROM_HEAP(pop, off)\
	((off) >= (pop)->heap_offset &&\
	(off) < (pop)->heap_offset + (pop)->heap_size)
#define OBJ_OFF_FROM_LANES(pop, off)\
	((off) >= (pop)->lanes_offset &&\
	(off) < (pop)->lanes_offset +\
	(pop)->nlanes * sizeof(struct lane_layout))

#define OBJ_PTR_FROM_POOL(pop, ptr)\
	((uintptr_t)(ptr) >= (uintptr_t)(pop) &&\
	(uintptr_t)(ptr) < (uintptr_t)(pop) + (pop)->size)

#define OBJ_OFF_IS_VALID(pop, off)\
	(OBJ_OFF_FROM_HEAP(pop, off) ||\
	(OBJ_PTR_TO_OFF(pop, &(pop)->root_offset) == (off)) ||\
	(OBJ_PTR_TO_OFF(pop, &(pop)->root_size) == (off)) ||\
	(OBJ_OFF_FROM_LANES(pop, off)))

#define OBJ_PTR_IS_VALID(pop, ptr)\
	OBJ_OFF_IS_VALID(pop, OBJ_PTR_TO_OFF(pop, ptr))

typedef void (*persist_local_fn)(const void *, size_t);
typedef void (*flush_local_fn)(const void *, size_t);
typedef void (*drain_local_fn)(void);
typedef void *(*memcpy_local_fn)(void *dest, const void *src, size_t len);
typedef void *(*memset_local_fn)(void *dest, int c, size_t len);

typedef void *(*persist_remote_fn)(PMEMobjpool *pop, const void *addr,
					size_t len, unsigned lane);

typedef uint64_t type_num_t;

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

	/* some run-time state, allocated out of memory pool... */
	void *addr;		/* mapped region */
	size_t size;		/* size of mapped region */
	int is_pmem;		/* true if pool is PMEM */
	int rdonly;		/* true if pool is opened read-only */
	struct palloc_heap heap;
	struct lane_descriptor lanes_desc;
	uint64_t uuid_lo;
	int is_dev_dax;		/* true if mapped on device dax */

	struct ctl *ctl;
	struct ringbuf *tx_postcommit_tasks;

	struct pool_set *set;		/* pool set info */
	struct pmemobjpool *replica;	/* next replica */
	struct redo_ctx *redo;

	/* per-replica functions: pmem or non-pmem */
	persist_local_fn persist_local;	/* persist function */
	flush_local_fn flush_local;	/* flush function */
	drain_local_fn drain_local;	/* drain function */
	memcpy_local_fn memcpy_persist_local; /* persistent memcpy function */
	memset_local_fn memset_persist_local; /* persistent memset function */

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

	persist_remote_fn persist_remote; /* remote persist function */

	int vg_boot;
	int tx_debug_skip_expensive_checks;

	struct tx_parameters *tx_params;

	/* padding to align size of this structure to page boundary */
	/* sizeof(unused2) == 8192 - offsetof(struct pmemobjpool, unused2) */
	char unused2[1548];
};

/*
 * Stored in the 'size' field of oobh header, determines whether the object
 * is internal or not. Internal objects are skipped in pmemobj iteration
 * functions.
 */
#define OBJ_INTERNAL_OBJECT_MASK ((1ULL) << 15)

#define CLASS_ID_FROM_FLAG(flag)\
((uint16_t)(((flag) & 0xFFFF000000000000) >> 48))

/*
 * pmemobj_get_uuid_lo -- (internal) evaluates XOR sum of least significant
 * 8 bytes with most significant 8 bytes.
 */
static inline uint64_t
pmemobj_get_uuid_lo(PMEMobjpool *pop)
{
	uint64_t uuid_lo = 0;

	for (int i = 0; i < 8; i++) {
		uuid_lo = (uuid_lo << 8) |
			(pop->hdr.poolset_uuid[i] ^
				pop->hdr.poolset_uuid[8 + i]);
	}

	return uuid_lo;
}

/*
 * OBJ_OID_IS_VALID -- (internal) checks if 'oid' is valid
 */
static inline int
OBJ_OID_IS_VALID(PMEMobjpool *pop, PMEMoid oid)
{
	return OBJ_OID_IS_NULL(oid) ||
		(oid.pool_uuid_lo == pop->uuid_lo &&
		    oid.off >= pop->heap_offset &&
		    oid.off < pop->heap_offset + pop->heap_size);
}

void obj_init(void);
void obj_fini(void);
int obj_read_remote(void *ctx, uintptr_t base, void *dest, void *addr,
		size_t length);

/*
 * (debug helper macro) logs notice message if used inside a transaction
 */
#ifdef DEBUG
#define _POBJ_DEBUG_NOTICE_IN_TX()\
	_pobj_debug_notice(__func__, NULL, 0)
#else
#define _POBJ_DEBUG_NOTICE_IN_TX() do {} while (0)
#endif

#endif
