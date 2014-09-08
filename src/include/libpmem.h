/*
 * Copyright (c) 2014, Intel Corporation
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
 *     * Neither the name of Intel Corporation nor the names of its
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
 * libpmem.h -- definitions of libpmem entry points
 *
 * This library provides support for programming with Persistent Memory (PMEM).
 *
 * The libpmem entry points are divided below into these categories:
 *	- basic PMEM flush-to-durability support
 *	- support for memory allocation and transactions in PMEM
 *	- support for arrays of atomically-writable blocks
 *	- support for PMEM-resident log files
 *	- managing overall library behavior
 *
 * See libpmem(3) for details.
 */

#ifndef	LIBPMEM_H
#define	LIBPMEM_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/uio.h>

/*
 * opaque types internal to libpmem...
 */
typedef struct pmemtrn PMEMtrn;
typedef struct pmemblk PMEMblk;
typedef struct pmemlog PMEMlog;

/*
 * basic PMEM flush-to-durability support...
 */
int pmem_is_pmem(void *addr, size_t len);
void pmem_persist(void *addr, size_t len, int flags);
void pmem_flush(void *addr, size_t len, int flags);
void pmem_fence(void);
void pmem_drain(void);

/*
 * support for memory allocation and transactions in PMEM...
 */
#define	PMEMTRN_MIN_POOL ((size_t)(1024 * 1024 * 2)) /* min pool size: 2MB */
PMEMtrn *pmemtrn_map(int fd);
void pmemtrn_unmap(PMEMtrn *ptp);
void *pmemtrn_static_area(PMEMtrn *ptp);
int pmemtrn_begin(PMEMtrn *ptp);
int pmemtrn_commit(PMEMtrn *ptp, int tid);
int pmemtrn_abort(PMEMtrn *ptp, int tid);
void *pmemtrn_malloc(PMEMtrn *ptp, size_t size);
void pmemtrn_free(PMEMtrn *ptp, void *ptr);
void *pmemtrn_calloc(PMEMtrn *ptp, size_t nmemb, size_t size);
void *pmemtrn_realloc(PMEMtrn *ptp, void *ptr, size_t size);
void *pmemtrn_aligned_alloc(PMEMtrn *ptp, size_t alignment, size_t size);
char *pmemtrn_strdup(PMEMtrn *ptp, const char *s);
void *pmemtrn_alloc(PMEMtrn *ptp, void *oldptr, size_t alignment, int zeroed,
		int tag, size_t size);
void pmemtrn_set(PMEMtrn *ptp, void *pmem_dest, const void *src, size_t n);
void pmemtrn_set_ptr(PMEMtrn *ptp, void *pmem_dest, const void *src);
void *pmemtrn_ptr(PMEMtrn *ptp, void *pmem_src);
void pmemtrn_strncpy(PMEMtrn *ptp, char *pmem_dest, const char *src, size_t n);
void pmemtrn_walk(PMEMtrn *ptp,
		void (*cb)(const void *ptr, int tag, void *arg), void *arg);
PMEMtrn *pmemtrn_map_replicant(PMEMtrn *ptp, int fd);

/*
 * support for arrays of atomically-writable blocks...
 */
#define	PMEMBLK_MIN_POOL ((size_t)(1024 * 1024 * 1024)) /* min pool size: 1GB */
#define	PMEMBLK_MIN_BLK ((size_t)512)
PMEMblk *pmemblk_map(int fd, size_t bsize);
void pmemblk_unmap(PMEMblk *pbp);
size_t pmemblk_nblock(PMEMblk *pbp);
int pmemblk_read(PMEMblk *pbp, void *buf, off_t blockno);
int pmemblk_write(PMEMblk *pbp, const void *buf, off_t blockno);
int pmemblk_set_zero(PMEMblk *pbp, off_t blockno);
int pmemblk_set_error(PMEMblk *pbp, off_t blockno);

/*
 * support for PMEM-resident log files...
 */
#define	PMEMLOG_MIN_POOL ((size_t)(1024 * 1024 * 2)) /* min pool size: 2MB */
PMEMlog *pmemlog_map(int fd);
void pmemlog_unmap(PMEMlog *plp);
size_t pmemlog_nbyte(PMEMlog *plp);
int pmemlog_append(PMEMlog *plp, const void *buf, size_t count);
int pmemlog_appendv(PMEMlog *plp, const struct iovec *iov, int iovcnt);
off_t pmemlog_tell(PMEMlog *plp);
void pmemlog_rewind(PMEMlog *plp);
void pmemlog_walk(PMEMlog *plp, size_t chunksize,
	int (*process_chunk)(const void *buf, size_t len, void *arg),
	void *arg);

/*
 * managing overall library behavior...
 */

/*
 * PMEM_MAJOR_VERSION and PMEM_MINOR_VERSION provide the current
 * version of the libpmem API as provided by this header file.
 * Applications can verify that the version available at run-time
 * is compatible with the version used at compile-time by passing
 * these defines to pmem_check_version().
 */
#define	PMEM_MAJOR_VERSION 1
#define	PMEM_MINOR_VERSION 0
const char *pmem_check_version(
		unsigned major_required,
		unsigned minor_required);

/*
 * Passing NULL to pmem_set_funcs() tells libpmem to continue to use
 * the default for that function.  The replacement functions must
 * not make calls back into libpmem.
 *
 * The print_func is called by libpmem based on the environment
 * variable PMEM_LOG_LEVEL:
 * 	0 or unset: print_func is only called for pmem_pool_stats_print()
 * 	1:          additional details are logged when errors are returned
 * 	2:          basic operations (allocations/frees) are logged
 * 	3:          produce very verbose tracing of function calls in libpmem
 * 	4:          also log obscure stuff used to debug the library itself
 *
 * The default print_func prints to stderr.  Applications can override this
 * by setting the environment variable PMEM_LOG_FILE, or by supplying a
 * replacement print function.
 */
void pmem_set_funcs(
		void *(*malloc_func)(size_t size),
		void (*free_func)(void *ptr),
		void *(*realloc_func)(void *ptr, size_t size),
		char *(*strdup_func)(const char *s),
		void (*print_func)(const char *s),
		void (*persist_func)(void *addr, size_t len, int flags));

/*
 * These are consistency checkers, for library debugging/testing, meant to
 * work on persistent memory files that are not current mapped or in use.
 */
int pmemtrn_check(const char *path);
int pmemblk_check(const char *path);
int pmemlog_check(const char *path);

#ifdef __cplusplus
}
#endif
#endif	/* libpmem.h */
