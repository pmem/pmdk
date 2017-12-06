/*
 * Copyright 2016-2017, Intel Corporation
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
 * cto.c -- memory pool & allocation entry points for libpmemcto
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <wchar.h>

#include "libpmemcto.h"
#include "libpmem.h"

#include "jemalloc.h"
#include "util.h"
#include "util_pmem.h"
#include "set.h"
#include "out.h"
#include "cto.h"
#include "mmap.h"
#include "sys_util.h"
#include "valgrind_internal.h"
#include "os_thread.h"

/*
 * cto_print_jemalloc_messages -- (internal) custom print function for jemalloc
 *
 * Prints traces from jemalloc. All traces from jemalloc are error messages.
 */
static void
cto_print_jemalloc_messages(void *ignore, const char *s)
{
	ERR("%s", s);
}

/*
 * cto_print_jemalloc_stats --(internal) print function for jemalloc statistics
 *
 * Prints statistics from jemalloc. All statistics are printed with level 0.
 */
static void
cto_print_jemalloc_stats(void *ignore, const char *s)
{
	LOG_NONL(0, "%s", s);
}

/*
 * cto_init -- load-time initialization for cto
 *
 * Called automatically by the run-time loader.
 */
void
cto_init(void)
{
	COMPILE_ERROR_ON(offsetof(struct pmemcto, set) !=
			POOL_HDR_SIZE + CTO_DSC_P_SIZE);

	/* set up jemalloc messages to a custom print function */
	je_cto_malloc_message = cto_print_jemalloc_messages;
}

/*
 * cto_fini -- libpmemcto cleanup routine
 *
 * Called automatically when the process terminates.
 */
void
cto_fini(void)
{
	LOG(3, NULL);
	/* nothing to be done */
}

/*
 * cto_descr_create -- (internal) create cto memory pool descriptor
 */
static int
cto_descr_create(PMEMctopool *pcp, const char *layout, size_t poolsize)
{
	LOG(3, "pcp %p layout \"%s\" poolsize %zu", pcp, layout, poolsize);

	ASSERTeq(poolsize % Pagesize, 0);

	/* opaque info lives at the beginning of mapped memory pool */
	void *dscp = (void *)((uintptr_t)pcp +
				sizeof(struct pool_hdr));

	/* create required metadata */
	memset(dscp, 0, CTO_DSC_P_SIZE);

	if (layout)
		strncpy(pcp->layout, layout, PMEMCTO_MAX_LAYOUT - 1);

	pcp->addr = (uint64_t)pcp;
	pcp->size = poolsize;
	pcp->root = (uint64_t)NULL;
	pcp->consistent = 0;

	/* store non-volatile part of pool's descriptor */
	util_persist(pcp->is_pmem, dscp, CTO_DSC_P_SIZE);

	return 0;
}

/*
 * cto_descr_check -- (internal) validate cto pool descriptor
 */
static int
cto_descr_check(PMEMctopool *pcp, const char *layout, size_t poolsize)
{
	LOG(3, "pcp %p layout \"%s\" poolsize %zu", pcp, layout, poolsize);

	if (layout && strncmp(pcp->layout, layout, PMEMCTO_MAX_LAYOUT)) {
		ERR("wrong layout (\"%s\") pool created with layout \"%s\"",
			layout, pcp->layout);
		errno = EINVAL;
		return -1;
	}

	if (pcp->consistent == 0) {
		ERR("inconsistent pool");
		errno = EINVAL;
		return -1;
	}

	if ((void *)pcp->addr == NULL) {
		ERR("invalid mapping address");
		errno = EINVAL;
		return -1;
	}

	/*
	 * The pool could be created using older version of the library, when
	 * the minimum pool size was different.
	 */
	if (pcp->size < PMEMCTO_MIN_POOL) {
		LOG(4, "mapping size is less than minimum (%zu < %zu)",
				pcp->size, PMEMCTO_MIN_POOL);
	}

	if (pcp->size != poolsize) {
		ERR("mapping size does not match pool size: %zu != %zu",
				pcp->size, poolsize);
		errno = EINVAL;
		return -1;
	}

	if ((void *)pcp->root != NULL &&
	    ((char *)pcp->root < ((char *)pcp->addr + CTO_DSC_SIZE_ALIGNED) ||
	    (char *)pcp->root >= ((char *)pcp->addr + pcp->size))) {
		ERR("invalid root pointer");
		errno = EINVAL;
		return -1;
	}

	LOG(4, "addr %p size %zu root %p", (void *)pcp->addr, pcp->size,
			(void *)pcp->root);

	return 0;
}

/*
 * cto_runtime_init -- (internal) initialize cto memory pool runtime data
 */
static int
cto_runtime_init(PMEMctopool *pcp, int rdonly, int is_pmem)
{
	LOG(3, "pcp %p rdonly %d is_pmem %d", pcp, rdonly, is_pmem);

	/* reset consistency flag */
	pcp->consistent = 0;
	util_persist(pcp->is_pmem, (void *)pcp->addr, sizeof(struct pmemcto));

	/*
	 * If possible, turn off all permissions on the pool header page.
	 *
	 * The prototype PMFS doesn't allow this when large pages are in
	 * use. It is not considered an error if this fails.
	 */
	RANGE_NONE((void *)pcp->addr, sizeof(struct pool_hdr), pcp->is_dev_dax);

	return 0;
}

/*
 * pmemcto_create -- create a cto memory pool
 */
#ifndef _WIN32
static inline
#endif
PMEMctopool *
pmemcto_createU(const char *path, const char *layout, size_t poolsize,
		mode_t mode)
{
	LOG(3, "path \"%s\" layout \"%s\" poolsize %zu mode %o",
			path, layout, poolsize, mode);

	struct pool_set *set;

	/* check length of layout */
	if (layout && (strlen(layout) >= PMEMCTO_MAX_LAYOUT)) {
		ERR("Layout too long");
		errno = EINVAL;
		return NULL;
	}

	if (util_pool_create(&set, path,
			poolsize, PMEMCTO_MIN_POOL, PMEMCTO_MIN_PART,
			CTO_HDR_SIG, CTO_FORMAT_MAJOR,
			CTO_FORMAT_COMPAT_DEFAULT, CTO_FORMAT_INCOMPAT_DEFAULT,
			CTO_FORMAT_RO_COMPAT_DEFAULT, NULL,
			REPLICAS_DISABLED) != 0) {
		LOG(2, "cannot create pool or pool set");
		return NULL;
	}

	ASSERT(set->nreplicas > 0);

	struct pool_replica *rep = set->replica[0];
	PMEMctopool *pcp = rep->part[0].addr;

	VALGRIND_REMOVE_PMEM_MAPPING(&pcp->addr,
			sizeof(struct pmemcto) -
			((uintptr_t)&pcp->addr - (uintptr_t)&pcp->hdr));

	pcp->set = set;
	pcp->is_pmem = rep->is_pmem;
	pcp->is_dev_dax = rep->part[0].is_dev_dax;

	/* is_dev_dax implies is_pmem */
	ASSERT(!pcp->is_dev_dax || pcp->is_pmem);

	if (set->nreplicas > 1) {
		errno = ENOTSUP;
		ERR("!replicas not supported");
		goto err;
	}

	/* create pool descriptor */
	if (cto_descr_create(pcp, layout, rep->repsize) != 0) {
		LOG(2, "descriptor creation failed");
		goto err;
	}

	/* initialize runtime parts */
	if (cto_runtime_init(pcp, 0, rep->is_pmem) != 0) {
		ERR("pool initialization failed");
		goto err;
	}

	/* Prepare pool for jemalloc - empty */
	if (je_cto_pool_create(
			(void *)((uintptr_t)pcp + CTO_DSC_SIZE_ALIGNED),
			rep->repsize - CTO_DSC_SIZE_ALIGNED,
			set->zeroed, 1) == NULL) {
		ERR("pool creation failed");
		goto err;
	}

	if (util_poolset_chmod(set, mode))
		goto err;

	util_poolset_fdclose(set);

	LOG(3, "pcp %p", pcp);
	return pcp;

err:
	LOG(4, "error clean up");
	int oerrno = errno;
	util_poolset_close(set, DELETE_CREATED_PARTS);
	errno = oerrno;
	return NULL;
}

#ifndef _WIN32
/*
 * pmemcto_create -- create a log memory pool
 */
PMEMctopool *
pmemcto_create(const char *path, const char *layout, size_t poolsize,
		mode_t mode)
{
	return pmemcto_createU(path, layout, poolsize, mode);
}
#else
/*
 * pmemcto_createW -- create a log memory pool
 */
PMEMctopool *
pmemcto_createW(const wchar_t *path, const wchar_t *layout, size_t poolsize,
		mode_t mode)
{
	char *upath = util_toUTF8(path);
	if (upath == NULL)
		return NULL;
	char *ulayout = NULL;
	if (layout != NULL) {
		ulayout = util_toUTF8(layout);
		if (ulayout == NULL) {
			util_free_UTF8(upath);
			return NULL;
		}
	}
	PMEMctopool *ret = pmemcto_createU(upath, ulayout, poolsize, mode);

	util_free_UTF8(upath);
	util_free_UTF8(ulayout);

	return ret;
}
#endif

/*
 * cto_open_noinit -- (internal) open a cto memory pool w/o initialization
 *
 * This routine opens the pool, but does not any run-time initialization.
 */
static PMEMctopool *
cto_open_noinit(const char *path, const char *layout, int cow, void *addr)
{
	LOG(3, "path \"%s\" layout \"%s\" cow %d addr %p",
			path, layout, cow, addr);

	struct pool_set *set;

	if (util_pool_open(&set, path, cow, PMEMCTO_MIN_POOL,
			CTO_HDR_SIG, CTO_FORMAT_MAJOR,
			CTO_FORMAT_COMPAT_CHECK, CTO_FORMAT_INCOMPAT_CHECK,
			CTO_FORMAT_RO_COMPAT_CHECK, NULL, addr) != 0) {
		LOG(2, "cannot open pool or pool set");
		return NULL;
	}

	ASSERT(set->nreplicas > 0);

	struct pool_replica *rep = set->replica[0];
	PMEMctopool *pcp = rep->part[0].addr;

	VALGRIND_REMOVE_PMEM_MAPPING(&pcp->addr,
			sizeof(struct pmemcto) -
			((uintptr_t)&pcp->addr - (uintptr_t)&pcp->hdr));

	ASSERTeq(pcp->size, rep->repsize);
	pcp->set = set;
	pcp->is_pmem = rep->is_pmem;
	pcp->is_dev_dax = rep->part[0].is_dev_dax;

	/* is_dev_dax implies is_pmem */
	ASSERT(!pcp->is_dev_dax || pcp->is_pmem);

	if (set->nreplicas > 1) {
		errno = ENOTSUP;
		ERR("!replicas not supported");
		goto err;
	}

	/* validate pool descriptor */
	if (cto_descr_check(pcp, layout, set->poolsize) != 0) {
		LOG(2, "descriptor check failed");
		goto err;
	}

	util_poolset_fdclose(set);

	LOG(3, "pcp %p", pcp);
	return pcp;

err:
	LOG(4, "error clean up");
	int oerrno = errno;
	util_poolset_close(set, DO_NOT_DELETE_PARTS);
	errno = oerrno;
	return NULL;
}

/*
 * cto_open_common -- (internal) open a cto memory pool
 *
 * This routine does all the work, but takes a cow flag so internal
 * calls can map a read-only pool if required.
 */
static PMEMctopool *
cto_open_common(const char *path, const char *layout, int cow)
{
	LOG(3, "path \"%s\" layout \"%s\" cow %d", path, layout, cow);

	PMEMctopool *pcp;

	/*
	 * XXX: Opening/mapping the pool twice is not the coolest solution,
	 * but it makes it easier to support both single-file pools and
	 * pool sets.
	 */

	/* open pool set to check consistency and to get the mapping address */
	if ((pcp = cto_open_noinit(path, layout, cow, NULL)) == NULL) {
		LOG(2, "cannot open pool or pool set");
		return NULL;
	}

	/* get the last mapping address */
	void *mapaddr = (void *)pcp->addr;
	LOG(4, "mapping address: %p", mapaddr);

	int oerrno = errno;
	util_poolset_close(pcp->set, DO_NOT_DELETE_PARTS);
	errno = oerrno;

	/* open the pool once again using the mapping address as a hint */
	if ((pcp = cto_open_noinit(path, layout, cow, mapaddr)) == NULL) {
		LOG(2, "cannot open pool or pool set");
		return NULL;
	}

	struct pool_set *set = pcp->set;

	if ((void *)pcp->addr != pcp) {
		ERR("cannot mmap at the same address: %p != %p",
				pcp, (void *)pcp->addr);
		errno = ENOMEM;
		goto err;
	}

	/* initialize runtime parts */
	if (cto_runtime_init(pcp, set->rdonly, set->replica[0]->is_pmem) != 0) {
		ERR("pool initialization failed");
		goto err;
	}

	/*
	 * Initially, treat this memory region as undefined.
	 * Once jemalloc initializes its metadata, it will also mark
	 * registered free chunks (usable heap space) as unaddressable.
	 */
	VALGRIND_DO_MAKE_MEM_UNDEFINED(
			(void *)((uintptr_t)pcp + CTO_DSC_SIZE_ALIGNED),
			set->poolsize - CTO_DSC_SIZE_ALIGNED);

	/* Prepare pool for jemalloc */
	if (je_cto_pool_create(
			(void *)((uintptr_t)pcp + CTO_DSC_SIZE_ALIGNED),
			set->poolsize - CTO_DSC_SIZE_ALIGNED, 0, 0) == NULL) {
		ERR("pool creation failed");
		util_unmap((void *)pcp->addr, pcp->size);
		return NULL;
	}

	util_poolset_fdclose(set);

	LOG(3, "pcp %p", pcp);
	return pcp;

err:
	LOG(4, "error clean up");
	oerrno = errno;
	util_poolset_close(set, DO_NOT_DELETE_PARTS);
	errno = oerrno;
	return NULL;
}

#ifndef _WIN32
/*
 * pmemcto_open -- open an existing log memory pool
 */
PMEMctopool *
pmemcto_open(const char *path, const char *layout)
{
	LOG(3, "path \"%s\" layout \"%s\"", path, layout);

	return cto_open_common(path, layout, 0);
}
#else
/*
 * pmemcto_openU -- open an existing cto memory pool
 */
PMEMctopool *
pmemcto_openU(const char *path, const char *layout)
{
	LOG(3, "path \"%s\" layout \"%s\"", path, layout);

	return cto_open_common(path, layout, 0);
}

/*
 * pmemcto_openW -- open an existing log memory pool
 */
PMEMctopool *
pmemcto_openW(const wchar_t *path, const wchar_t *layout)
{
	char *upath = util_toUTF8(path);
	if (upath == NULL)
		return NULL;

	char *ulayout = NULL;
	if (layout != NULL) {
		ulayout = util_toUTF8(layout);
		if (ulayout == NULL) {
			util_free_UTF8(upath);
			return NULL;
		}
	}

	PMEMctopool *ret = pmemcto_openU(upath, ulayout);
	util_free_UTF8(upath);
	util_free_UTF8(ulayout);
	return ret;
}
#endif

/*
 * pmemcto_close -- close a cto memory pool
 */
void
pmemcto_close(PMEMctopool *pcp)
{
	LOG(3, "pcp %p", pcp);

	int ret = je_cto_pool_delete(
			(pool_t *)((uintptr_t)pcp + CTO_DSC_SIZE_ALIGNED));
	if (ret != 0) {
		ERR("invalid pool handle: %p", pcp);
		errno = EINVAL;
		return;
	}

	/* deep flush the entire pool to persistence */

	/* XXX: replace with pmem_deep_flush() when available */
	RANGE_RW((void *)pcp->addr, sizeof(struct pool_hdr), pcp->is_dev_dax);
	VALGRIND_DO_MAKE_MEM_DEFINED(pcp->addr, pcp->size);
	util_persist(pcp->is_pmem, (void *)pcp->addr, pcp->size);

	/* set consistency flag */
	pcp->consistent = 1;
	util_persist(pcp->is_pmem, &pcp->consistent, sizeof(pcp->consistent));

	util_poolset_close(pcp->set, DO_NOT_DELETE_PARTS);
}

/*
 * pmemcto_set_root_pointer -- saves pointer to root object
 */
void
pmemcto_set_root_pointer(PMEMctopool *pcp, void *ptr)
{
	LOG(3, "pcp %p ptr %p", pcp, ptr);

#ifdef DEBUG
	/* XXX: an error also in non-debug build? (return 0 or -1) */
	ASSERT(ptr == NULL ||
		((char *)ptr >= ((char *)pcp->addr + CTO_DSC_SIZE_ALIGNED) &&
		(char *)ptr < ((char *)pcp->addr + pcp->size)));
#endif

	pcp->root = (uint64_t)ptr;
}

/*
 * pmemcto_get_root_pointer -- returns pointer to root object
 */
void *
pmemcto_get_root_pointer(PMEMctopool *pcp)
{
	LOG(3, "pcp %p", pcp);

	LOG(4, "root ptr %p", (void *)pcp->root);
	return (void *)pcp->root;
}

/*
 * pmemcto_checkU -- memory pool consistency check
 */
#ifndef _WIN32
static inline
#endif
int
pmemcto_checkU(const char *path, const char *layout)
{
	LOG(3, "path \"%s\" layout \"%s\"", path, layout);

	PMEMctopool *pcp = cto_open_common(path, layout, 1);
	if (pcp == NULL)
		return -1;	/* errno set by pmemcto_open_common() */

	int consistent = je_cto_pool_check(
			(pool_t *)((uintptr_t)pcp + CTO_DSC_SIZE_ALIGNED));

	pmemcto_close(pcp);

	if (consistent)
		LOG(4, "pool consistency check OK");

	return consistent;
}

#ifndef _WIN32
/*
 * pmemcto_check -- cto memory pool consistency check
 *
 * Returns true if consistent, zero if inconsistent, -1/error if checking
 * cannot happen due to other errors.
 */
int
pmemcto_check(const char *path, const char *layout)
{
	return pmemcto_checkU(path, layout);
}
#else
/*
 * pmemcto_checkW -- cto memory pool consistency check
 */
int
pmemcto_checkW(const wchar_t *path, const wchar_t *layout)
{
	char *upath = util_toUTF8(path);
	if (upath == NULL)
		return -1;

	char *ulayout = NULL;
	if (layout != NULL) {
		ulayout = util_toUTF8(layout);
		if (ulayout == NULL) {
			util_free_UTF8(upath);
			return -1;
		}
	}

	int ret = pmemcto_checkU(upath, ulayout);

	util_free_UTF8(upath);
	util_free_UTF8(ulayout);
	return ret;
}
#endif

/*
 * pmemcto_stats_print -- spew memory allocator stats for a pool
 */
void
pmemcto_stats_print(PMEMctopool *pcp, const char *opts)
{
	LOG(3, "vmp %p opts \"%s\"", pcp, opts ? opts : "");

	je_cto_pool_malloc_stats_print(
			(pool_t *)((uintptr_t)pcp + CTO_DSC_SIZE_ALIGNED),
			cto_print_jemalloc_stats, NULL, opts);
}

/*
 * pmemcto_malloc -- allocate memory
 */
void *
pmemcto_malloc(PMEMctopool *pcp, size_t size)
{
	LOG(3, "pcp %p size %zu", pcp, size);

	return je_cto_pool_malloc(
			(pool_t *)((uintptr_t)pcp + CTO_DSC_SIZE_ALIGNED),
			size);
}

/*
 * pmemcto_free -- free memory
 */
void
pmemcto_free(PMEMctopool *pcp, void *ptr)
{
	LOG(3, "pcp %p ptr %p", pcp, ptr);

	je_cto_pool_free((pool_t *)(
			(uintptr_t)pcp + CTO_DSC_SIZE_ALIGNED), ptr);
}

/*
 * pmemcto_calloc -- allocate zeroed memory
 */
void *
pmemcto_calloc(PMEMctopool *pcp, size_t nmemb, size_t size)
{
	LOG(3, "pcp %p nmemb %zu size %zu", pcp, nmemb, size);

	return je_cto_pool_calloc(
			(pool_t *)((uintptr_t)pcp + CTO_DSC_SIZE_ALIGNED),
			nmemb, size);
}

/*
 * pmemcto_realloc -- resize a memory allocation
 */
void *
pmemcto_realloc(PMEMctopool *pcp, void *ptr, size_t size)
{
	LOG(3, "pcp %p ptr %p size %zu", pcp, ptr, size);

	return je_cto_pool_ralloc(
			(pool_t *)((uintptr_t)pcp + CTO_DSC_SIZE_ALIGNED),
			ptr, size);
}

/*
 * pmemcto_aligned_alloc -- allocate aligned memory
 */
void *
pmemcto_aligned_alloc(PMEMctopool *pcp, size_t alignment, size_t size)
{
	LOG(3, "pcp %p alignment %zu size %zu", pcp, alignment, size);

	return je_cto_pool_aligned_alloc(
			(pool_t *)((uintptr_t)pcp + CTO_DSC_SIZE_ALIGNED),
			alignment, size);
}

/*
 * pmemcto_strdup -- allocate memory for copy of string
 */
char *
pmemcto_strdup(PMEMctopool *pcp, const char *s)
{
	LOG(3, "pcp %p s %p", pcp, s);

	size_t size = strlen(s) + 1;
	void *retaddr = je_cto_pool_malloc(
			(pool_t *)((uintptr_t)pcp + CTO_DSC_SIZE_ALIGNED),
			size);
	if (retaddr == NULL)
		return NULL;

	return (char *)memcpy(retaddr, s, size);
}

/*
 * pmemcto_wcsdup -- allocate memory for copy of widechar string
 */
wchar_t *
pmemcto_wcsdup(PMEMctopool *pcp, const wchar_t *s)
{
	LOG(3, "pcp %p s %p", pcp, s);

	size_t size = (wcslen(s) + 1) * sizeof(wchar_t);
	void *retaddr = je_cto_pool_malloc(
			(pool_t *)((uintptr_t)pcp + CTO_DSC_SIZE_ALIGNED),
			size);
	if (retaddr == NULL)
		return NULL;

	return (wchar_t *)memcpy(retaddr, s, size);
}

/*
 * pmemcto_malloc_usable_size -- get usable size of allocation
 */
size_t
pmemcto_malloc_usable_size(PMEMctopool *pcp, void *ptr)
{
	LOG(3, "pcp %p ptr %p", pcp, ptr);

	return je_cto_pool_malloc_usable_size(
			(pool_t *)((uintptr_t)pcp + CTO_DSC_SIZE_ALIGNED), ptr);
}
