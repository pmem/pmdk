/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2014-2022, Intel Corporation */

/*
 * blk.h -- internal definitions for libpmem blk module
 */

#ifndef BLK_H
#define BLK_H 1

#include <stddef.h>

#include "ctl.h"
#include "os_thread.h"
#include "pool_hdr.h"
#include "page_size.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "alloc.h"
#include "fault_injection.h"

#define PMEMBLK_LOG_PREFIX "libpmemblk"
#define PMEMBLK_LOG_LEVEL_VAR "PMEMBLK_LOG_LEVEL"
#define PMEMBLK_LOG_FILE_VAR "PMEMBLK_LOG_FILE"

/* attributes of the blk memory pool format for the pool header */
#define BLK_HDR_SIG "PMEMBLK"	/* must be 8 bytes including '\0' */
#define BLK_FORMAT_MAJOR 1

#define BLK_FORMAT_FEAT_DEFAULT \
	{POOL_FEAT_COMPAT_DEFAULT, POOL_FEAT_INCOMPAT_DEFAULT, 0x0000}

#define BLK_FORMAT_FEAT_CHECK \
	{POOL_FEAT_COMPAT_VALID, POOL_FEAT_INCOMPAT_VALID, 0x0000}

static const features_t blk_format_feat_default = BLK_FORMAT_FEAT_DEFAULT;

struct pmemblk {
	struct pool_hdr hdr;	/* memory pool header */

	/* root info for on-media format... */
	uint32_t bsize;		/* block size */

	/* flag indicating if the pool was zero-initialized */
	int is_zeroed;

	/* some run-time state, allocated out of memory pool... */
	void *addr;		/* mapped region */
	size_t size;		/* size of mapped region */
	int is_pmem;		/* true if pool is PMEM */
	int rdonly;		/* true if pool is opened read-only */
	void *data;		/* post-header data area */
	size_t datasize;	/* size of data area */
	size_t nlba;		/* number of LBAs in pool */
	struct btt *bttp;	/* btt handle */
	unsigned nlane;		/* number of lanes */
	unsigned next_lane;	/* used to rotate through lanes */
	os_mutex_t *locks;	/* one per lane */
	int is_dev_dax;		/* true if mapped on device dax */
	struct ctl *ctl;	/* top level node of the ctl tree structure */

	struct pool_set *set;	/* pool set info */

	struct vdm *vdm; /* miniasync abstraction for virtual data mover */

#ifdef DEBUG
	/* held during read/write mprotected sections */
	os_mutex_t write_lock;
#endif
};

/* data area starts at this alignment after the struct pmemblk above */
#define BLK_FORMAT_DATA_ALIGN ((uintptr_t)PMEM_PAGESIZE)

#if FAULT_INJECTION
void
pmemblk_inject_fault_at(enum pmem_allocation_type type, int nth,
							const char *at);

int
pmemblk_fault_injection_enabled(void);
#else
static inline void
pmemblk_inject_fault_at(enum pmem_allocation_type type, int nth,
						const char *at)
{
	/* suppress unused-parameter errors */
	SUPPRESS_UNUSED(type, nth, at);

	abort();
}

static inline int
pmemblk_fault_injection_enabled(void)
{
	return 0;
}
#endif

#ifdef __cplusplus
}
#endif

#endif
