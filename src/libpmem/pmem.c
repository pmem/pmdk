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
 * pmem.c -- pmem entry points for libpmem
 *
 *
 * PERSISTENT MEMORY INSTRUCTIONS ON X86
 *
 * The primary feature of this library is to provide a way to flush
 * changes to persistent memory as outlined below (note that many
 * of the decisions below are made at initialization time, and not
 * repeated every time a flush is requested).
 *
 * To flush a range to pmem when CLWB is available:
 *
 *	CLWB for each cache line in the given range.
 *
 *	SFENCE to ensure the CLWBs above have completed.
 *
 * To flush a range to pmem when CLFLUSHOPT is available and CLWB is not
 * (same as above but issue CLFLUSHOPT instead of CLWB):
 *
 *	CLFLUSHOPT for each cache line in the given range.
 *
 *	SFENCE to ensure the CLWBs above have completed.
 *
 * To flush a range to pmem when neither CLFLUSHOPT or CLWB are available
 * (same as above but fences surrounding CLFLUSH are not required):
 *
 *	CLFLUSH for each cache line in the given range.
 *
 * To memcpy a range of memory to pmem when MOVNT is available:
 *
 *	Copy any non-64-byte portion of the destination using MOV.
 *
 *	Use the flush flow above without the fence for the copied portion.
 *
 *	Copy using MOVNTDQ, up to any non-64-byte aligned end portion.
 *	(The MOVNT instructions bypass the cache, so no flush is required.)
 *
 *	Copy any unaligned end portion using MOV.
 *
 *	Use the flush flow above for the copied portion (including fence).
 *
 * To memcpy a range of memory to pmem when MOVNT is not available:
 *
 *	Just pass the call to the normal memcpy() followed by pmem_persist().
 *
 * To memset a non-trivial sized range of memory to pmem:
 *
 *	Same as the memcpy cases above but store the given value instead
 *	of reading values from the source.
 *
 * These features are supported for ARM AARCH64 using equivalent ARM
 * assembly instruction. Please refer to (arm_cacheops.h) for more details.
 *
 * INTERFACES FOR FLUSHING TO PERSISTENT MEMORY
 *
 * Given the flows above, three interfaces are provided for flushing a range
 * so that the caller has the ability to separate the steps when necessary,
 * but otherwise leaves the detection of available instructions to the libpmem:
 *
 * pmem_persist(addr, len)
 *
 *	This is the common case, which just calls the two other functions:
 *
 *		pmem_flush(addr, len);
 *		pmem_drain();
 *
 * pmem_flush(addr, len)
 *
 *	CLWB or CLFLUSHOPT or CLFLUSH for each cache line
 *
 * pmem_drain()
 *
 *	SFENCE unless using CLFLUSH
 *
 *
 * INTERFACES FOR COPYING/SETTING RANGES OF MEMORY
 *
 * Given the flows above, the following interfaces are provided for the
 * memmove/memcpy/memset operations to persistent memory:
 *
 * pmem_memmove_nodrain()
 *
 *	Checks for overlapped ranges to determine whether to copy from
 *	the beginning of the range or from the end.  If MOVNT instructions
 *	are available, uses the memory copy flow described above, otherwise
 *	calls the libc memmove() followed by pmem_flush(). Since no conditional
 *	compilation and/or architecture specific CFLAGS are in use at the
 *	moment, SSE2 ( thus movnt ) is just assumed to be available.
 *
 * pmem_memcpy_nodrain()
 *
 *	Just calls pmem_memmove_nodrain().
 *
 * pmem_memset_nodrain()
 *
 *	If MOVNT instructions are available, uses the memset flow described
 *	above, otherwise calls the libc memset() followed by pmem_flush().
 *
 * pmem_memmove_persist()
 * pmem_memcpy_persist()
 * pmem_memset_persist()
 *
 *	Calls the appropriate _nodrain() function followed by pmem_drain().
 *
 *
 * DECISIONS MADE AT INITIALIZATION TIME
 *
 * As much as possible, all decisions described above are made at library
 * initialization time.  This is achieved using function pointers that are
 * setup by pmem_init() when the library loads.
 *
 *	Func_predrain_fence is used by pmem_drain() to call one of:
 *		predrain_fence_empty()
 *		predrain_memory_barrier()
 *
 *	Func_flush is used by pmem_flush() to call one of:
 *		flush_dcache()
 *		flush_dcache_invalidate_opt()
 *		flush_dcache_invalidate()
 *
 *	Func_memmove_nodrain is used by memmove_nodrain() to call one of:
 *		memmove_nodrain_libc()
 *		memmove_nodrain_movnt()
 *
 *	Func_memset_nodrain is used by memset_nodrain() to call one of:
 *		memset_nodrain_libc()
 *		memset_nodrain_movnt()
 *
 * DEBUG LOGGING
 *
 * Many of the functions here get called hundreds of times from loops
 * iterating over ranges, making the usual LOG() calls at level 3
 * impractical.  The call tracing log for those functions is set at 15.
 */

#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>

#include "libpmem.h"
#include "pmem.h"
#include "out.h"
#include "os.h"
#include "mmap.h"
#include "file.h"
#include "valgrind_internal.h"
#include "os_deep.h"
#include "auto_flush.h"

static struct pmem_funcs Funcs;

/*
 * pmem_has_hw_drain -- return whether or not HW drain was found
 *
 * Always false for x86: HW drain is done by HW with no SW involvement.
 */
int
pmem_has_hw_drain(void)
{
	LOG(3, NULL);

	return 0;
}

/*
 * pmem_drain -- wait for any PM stores to drain from HW buffers
 */
void
pmem_drain(void)
{
	LOG(15, NULL);

	Funcs.predrain_fence();
}

/*
 * pmem_has_auto_flush -- check if platform supports eADR
 */
int
pmem_has_auto_flush()
{
	LOG(3, NULL);

	return pmem2_auto_flush();
}

/*
 * pmem_deep_flush -- flush processor cache for the given range
 * regardless of eADR support on platform
 */
void
pmem_deep_flush(const void *addr, size_t len)
{
	LOG(15, "addr %p len %zu", addr, len);

	VALGRIND_DO_CHECK_MEM_IS_ADDRESSABLE(addr, len);

	Funcs.deep_flush(addr, len);
}

/*
 * pmem_flush -- flush processor cache for the given range
 */
void
pmem_flush(const void *addr, size_t len)
{
	LOG(15, "addr %p len %zu", addr, len);

	VALGRIND_DO_CHECK_MEM_IS_ADDRESSABLE(addr, len);

	Funcs.flush(addr, len);
}

/*
 * pmem_persist -- make any cached changes to a range of pmem persistent
 */
void
pmem_persist(const void *addr, size_t len)
{
	LOG(15, "addr %p len %zu", addr, len);

	pmem_flush(addr, len);
	pmem_drain();
}

/*
 * pmem_msync -- flush to persistence via msync
 *
 * Using msync() means this routine is less optimal for pmem (but it
 * still works) but it also works for any memory mapped file, unlike
 * pmem_persist() which is only safe where pmem_is_pmem() returns true.
 */
int
pmem_msync(const void *addr, size_t len)
{
	LOG(15, "addr %p len %zu", addr, len);

	VALGRIND_DO_CHECK_MEM_IS_ADDRESSABLE(addr, len);

	/*
	 * msync requires addr to be a multiple of pagesize but there are no
	 * requirements for len. Align addr down and change len so that
	 * [addr, addr + len) still contains initial range.
	 */

	/* increase len by the amount we gain when we round addr down */
	len += (uintptr_t)addr & (Pagesize - 1);

	/* round addr down to page boundary */
	uintptr_t uptr = (uintptr_t)addr & ~((uintptr_t)Pagesize - 1);

	/*
	 * msync accepts addresses aligned to page boundary, so we may sync
	 * more and part of it may have been marked as undefined/inaccessible
	 * Msyncing such memory is not a bug, so as a workaround temporarily
	 * disable error reporting.
	 */
	VALGRIND_DO_DISABLE_ERROR_REPORTING;

	int ret;
	if ((ret = msync((void *)uptr, len, MS_SYNC)) < 0)
		ERR("!msync");

	VALGRIND_DO_ENABLE_ERROR_REPORTING;

	/* full flush */
	VALGRIND_DO_PERSIST(uptr, len);

	return ret;
}

/*
 * is_pmem_always -- (internal) always true (for meaningful parameters) version
 *	of pmem_is_pmem()
 */
static int
is_pmem_always(const void *addr, size_t len)
{
	LOG(3, "addr %p len %zu", addr, len);

	if (len == 0)
		return 0;

	return 1;
}

/*
 * is_pmem_never -- (internal) never true version of pmem_is_pmem()
 */
static int
is_pmem_never(const void *addr, size_t len)
{
	LOG(3, "addr %p len %zu", addr, len);

	return 0;
}

/*
 * pmem_is_pmem_init -- (internal) initialize Func_is_pmem pointer
 *
 * This should be done only once - on the first call to pmem_is_pmem().
 * If PMEM_IS_PMEM_FORCE is set, it would override the default behavior
 * of pmem_is_pmem().
 */
static void
pmem_is_pmem_init(void)
{
	LOG(3, NULL);

	static volatile unsigned init;

	while (init != 2) {
		if (!util_bool_compare_and_swap32(&init, 0, 1))
			continue;

		/*
		 * For debugging/testing, allow pmem_is_pmem() to be forced
		 * to always true or never true using environment variable
		 * PMEM_IS_PMEM_FORCE values of zero or one.
		 *
		 * This isn't #ifdef DEBUG because it has a trivial performance
		 * impact and it may turn out to be useful as a "chicken bit"
		 * for systems where pmem_is_pmem() isn't correctly detecting
		 * true persistent memory.
		 */
		char *ptr = os_getenv("PMEM_IS_PMEM_FORCE");
		if (ptr) {
			int val = atoi(ptr);

			if (val == 0)
				Funcs.is_pmem = is_pmem_never;
			else if (val == 1)
				Funcs.is_pmem = is_pmem_always;

			VALGRIND_ANNOTATE_HAPPENS_BEFORE(&Funcs.is_pmem);

			LOG(4, "PMEM_IS_PMEM_FORCE=%d", val);
		}

		if (Funcs.is_pmem == NULL)
			Funcs.is_pmem = is_pmem_never;

		if (!util_bool_compare_and_swap32(&init, 1, 2))
			FATAL("util_bool_compare_and_swap32");
	}
}

/*
 * pmem_is_pmem -- return true if entire range is persistent memory
 */
int
pmem_is_pmem(const void *addr, size_t len)
{
	LOG(10, "addr %p len %zu", addr, len);

	static int once;

	/* This is not thread-safe, but pmem_is_pmem_init() is. */
	if (once == 0) {
		pmem_is_pmem_init();
		util_fetch_and_add32(&once, 1);
	}

	VALGRIND_ANNOTATE_HAPPENS_AFTER(&Funcs.is_pmem);
	return Funcs.is_pmem(addr, len);
}

#define PMEM_FILE_ALL_FLAGS\
	(PMEM_FILE_CREATE|PMEM_FILE_EXCL|PMEM_FILE_SPARSE|PMEM_FILE_TMPFILE)

#define PMEM_DAX_VALID_FLAGS\
	(PMEM_FILE_CREATE|PMEM_FILE_SPARSE)

/*
 * pmem_map_fileU -- create or open the file and map it to memory
 */
#ifndef _WIN32
static inline
#endif
void *
pmem_map_fileU(const char *path, size_t len, int flags,
	mode_t mode, size_t *mapped_lenp, int *is_pmemp)
{
	LOG(3, "path \"%s\" size %zu flags %x mode %o mapped_lenp %p "
		"is_pmemp %p", path, len, flags, mode, mapped_lenp, is_pmemp);

	int oerrno;
	int fd;
	int open_flags = O_RDWR;
	int delete_on_err = 0;
	int file_type = util_file_get_type(path);
#ifdef _WIN32
	open_flags |= O_BINARY;
#endif

	if (file_type == OTHER_ERROR)
		return NULL;

	if (flags & ~(PMEM_FILE_ALL_FLAGS)) {
		ERR("invalid flag specified %x", flags);
		errno = EINVAL;
		return NULL;
	}

	if (file_type == TYPE_DEVDAX) {
		if (flags & ~(PMEM_DAX_VALID_FLAGS)) {
			ERR("flag unsupported for Device DAX %x", flags);
			errno = EINVAL;
			return NULL;
		} else {
			/* we are ignoring all of the flags */
			flags = 0;
			ssize_t actual_len = util_file_get_size(path);
			if (actual_len < 0) {
				ERR("unable to read Device DAX size");
				errno = EINVAL;
				return NULL;
			}
			if (len != 0 && len != (size_t)actual_len) {
				ERR("Device DAX length must be either 0 or "
					"the exact size of the device: %zu",
					actual_len);
				errno = EINVAL;
				return NULL;
			}
			len = 0;
		}
	}

	if (flags & PMEM_FILE_CREATE) {
		if ((os_off_t)len < 0) {
			ERR("invalid file length %zu", len);
			errno = EINVAL;
			return NULL;
		}
		open_flags |= O_CREAT;
	}

	if (flags & PMEM_FILE_EXCL)
		open_flags |= O_EXCL;

	if ((len != 0) && !(flags & PMEM_FILE_CREATE)) {
		ERR("non-zero 'len' not allowed without PMEM_FILE_CREATE");
		errno = EINVAL;
		return NULL;
	}

	if ((len == 0) && (flags & PMEM_FILE_CREATE)) {
		ERR("zero 'len' not allowed with PMEM_FILE_CREATE");
		errno = EINVAL;
		return NULL;
	}

	if ((flags & PMEM_FILE_TMPFILE) && !(flags & PMEM_FILE_CREATE)) {
		ERR("PMEM_FILE_TMPFILE not allowed without PMEM_FILE_CREATE");
		errno = EINVAL;
		return NULL;
	}

	if (flags & PMEM_FILE_TMPFILE) {
		if ((fd = util_tmpfile(path,
					OS_DIR_SEP_STR"pmem.XXXXXX",
					open_flags & O_EXCL)) < 0) {
			LOG(2, "failed to create temporary file at \"%s\"",
				path);
			return NULL;
		}
	} else {
		if ((fd = os_open(path, open_flags, mode)) < 0) {
			ERR("!open %s", path);
			return NULL;
		}
		if ((flags & PMEM_FILE_CREATE) && (flags & PMEM_FILE_EXCL))
			delete_on_err = 1;
	}

	if (flags & PMEM_FILE_CREATE) {
		/*
		 * Always set length of file to 'len'.
		 * (May either extend or truncate existing file.)
		 */
		if (os_ftruncate(fd, (os_off_t)len) != 0) {
			ERR("!ftruncate");
			goto err;
		}
		if ((flags & PMEM_FILE_SPARSE) == 0) {
			if ((errno = os_posix_fallocate(fd, 0,
							(os_off_t)len)) != 0) {
				ERR("!posix_fallocate");
				goto err;
			}
		}
	} else {
		ssize_t actual_size = util_fd_get_size(fd);
		if (actual_size < 0) {
			ERR("stat %s: negative size", path);
			errno = EINVAL;
			goto err;
		}

		len = (size_t)actual_size;
	}

	void *addr = pmem_map_register(fd, len, path, file_type == TYPE_DEVDAX);
	if (addr == NULL)
		goto err;

	if (mapped_lenp != NULL)
		*mapped_lenp = len;

	if (is_pmemp != NULL)
		*is_pmemp = pmem_is_pmem(addr, len);

	LOG(3, "returning %p", addr);

	VALGRIND_REGISTER_PMEM_MAPPING(addr, len);
	VALGRIND_REGISTER_PMEM_FILE(fd, addr, len, 0);

	(void) os_close(fd);

	return addr;
err:
	oerrno = errno;
	(void) os_close(fd);
	if (delete_on_err)
		(void) os_unlink(path);
	errno = oerrno;
	return NULL;
}

#ifndef _WIN32
/*
 * pmem_map_file -- create or open the file and map it to memory
 */
void *
pmem_map_file(const char *path, size_t len, int flags,
	mode_t mode, size_t *mapped_lenp, int *is_pmemp)
{
	return pmem_map_fileU(path, len, flags, mode, mapped_lenp, is_pmemp);
}
#else
/*
 * pmem_map_fileW -- create or open the file and map it to memory
 */
void *
pmem_map_fileW(const wchar_t *path, size_t len, int flags, mode_t mode,
		size_t *mapped_lenp, int *is_pmemp) {
	char *upath = util_toUTF8(path);
	if (upath == NULL)
		return NULL;

	void *ret = pmem_map_fileU(upath, len, flags, mode, mapped_lenp,
					is_pmemp);

	util_free_UTF8(upath);
	return ret;
}
#endif

/*
 * pmem_unmap -- unmap the specified region
 */
int
pmem_unmap(void *addr, size_t len)
{
	LOG(3, "addr %p len %zu", addr, len);

#ifndef _WIN32
	util_range_unregister(addr, len);
#endif
	VALGRIND_REMOVE_PMEM_MAPPING(addr, len);
	return util_unmap(addr, len);
}

/*
 * pmem_memmove --  memmove to pmem
 */
void *
pmem_memmove(void *pmemdest, const void *src, size_t len, unsigned flags)
{
	LOG(15, "pmemdest %p src %p len %zu flags 0x%x",
			pmemdest, src, len, flags);

#ifdef DEBUG
	if (flags & ~PMEM_F_MEM_VALID_FLAGS)
		ERR("invalid flags 0x%x", flags);
#endif
	PMEM_API_START();
	Funcs.memmove_nodrain(pmemdest, src, len, flags & ~PMEM_F_MEM_NODRAIN);

	if ((flags & (PMEM_F_MEM_NODRAIN | PMEM_F_MEM_NOFLUSH)) == 0)
		pmem_drain();

	PMEM_API_END();
	return pmemdest;
}

/*
 * pmem_memcpy --  memcpy to pmem
 */
void *
pmem_memcpy(void *pmemdest, const void *src, size_t len, unsigned flags)
{
	LOG(15, "pmemdest %p src %p len %zu flags 0x%x",
			pmemdest, src, len, flags);

#ifdef DEBUG
	if (flags & ~PMEM_F_MEM_VALID_FLAGS)
		ERR("invalid flags 0x%x", flags);
#endif
	PMEM_API_START();
	Funcs.memmove_nodrain(pmemdest, src, len, flags & ~PMEM_F_MEM_NODRAIN);

	if ((flags & (PMEM_F_MEM_NODRAIN | PMEM_F_MEM_NOFLUSH)) == 0)
		pmem_drain();

	PMEM_API_END();
	return pmemdest;
}

/*
 * pmem_memset -- memset to pmem
 */
void *
pmem_memset(void *pmemdest, int c, size_t len, unsigned flags)
{
	LOG(15, "pmemdest %p c 0x%x len %zu flags 0x%x",
			pmemdest, c, len, flags);

#ifdef DEBUG
	if (flags & ~PMEM_F_MEM_VALID_FLAGS)
		ERR("invalid flags 0x%x", flags);
#endif

	PMEM_API_START();
	Funcs.memset_nodrain(pmemdest, c, len, flags & ~PMEM_F_MEM_NODRAIN);

	if ((flags & (PMEM_F_MEM_NODRAIN | PMEM_F_MEM_NOFLUSH)) == 0)
		pmem_drain();

	PMEM_API_END();
	return pmemdest;
}

/*
 * pmem_memmove_nodrain -- memmove to pmem without hw drain
 */
void *
pmem_memmove_nodrain(void *pmemdest, const void *src, size_t len)
{
	LOG(15, "pmemdest %p src %p len %zu", pmemdest, src, len);

	PMEM_API_START();

	Funcs.memmove_nodrain(pmemdest, src, len, 0);

	PMEM_API_END();
	return pmemdest;
}

/*
 * pmem_memcpy_nodrain -- memcpy to pmem without hw drain
 */
void *
pmem_memcpy_nodrain(void *pmemdest, const void *src, size_t len)
{
	LOG(15, "pmemdest %p src %p len %zu", pmemdest, src, len);

	PMEM_API_START();

	Funcs.memmove_nodrain(pmemdest, src, len, 0);

	PMEM_API_END();
	return pmemdest;
}

/*
 * pmem_memmove_persist -- memmove to pmem
 */
void *
pmem_memmove_persist(void *pmemdest, const void *src, size_t len)
{
	LOG(15, "pmemdest %p src %p len %zu", pmemdest, src, len);

	PMEM_API_START();

	Funcs.memmove_nodrain(pmemdest, src, len, 0);
	pmem_drain();

	PMEM_API_END();
	return pmemdest;
}

/*
 * pmem_memcpy_persist -- memcpy to pmem
 */
void *
pmem_memcpy_persist(void *pmemdest, const void *src, size_t len)
{
	LOG(15, "pmemdest %p src %p len %zu", pmemdest, src, len);

	PMEM_API_START();

	Funcs.memmove_nodrain(pmemdest, src, len, 0);
	pmem_drain();

	PMEM_API_END();
	return pmemdest;
}

/*
 * pmem_memset_nodrain -- memset to pmem without hw drain
 */
void *
pmem_memset_nodrain(void *pmemdest, int c, size_t len)
{
	LOG(15, "pmemdest %p c %d len %zu", pmemdest, c, len);

	PMEM_API_START();

	Funcs.memset_nodrain(pmemdest, c, len, 0);

	PMEM_API_END();
	return pmemdest;
}

/*
 * pmem_memset_persist -- memset to pmem
 */
void *
pmem_memset_persist(void *pmemdest, int c, size_t len)
{
	LOG(15, "pmemdest %p c %d len %zu", pmemdest, c, len);

	PMEM_API_START();

	Funcs.memset_nodrain(pmemdest, c, len, 0);
	pmem_drain();

	PMEM_API_END();
	return pmemdest;
}

/*
 * pmem_init -- load-time initialization for pmem.c
 */
void
pmem_init(void)
{
	LOG(3, NULL);

	pmem_init_funcs(&Funcs);
	pmem_os_init();
}

/*
 * pmem_deep_persist -- perform deep persist on a memory range
 *
 * It merely acts as wrapper around an msync call in most cases, the only
 * exception is the case of an mmap'ed DAX device on Linux.
 */
int
pmem_deep_persist(const void *addr, size_t len)
{
	LOG(3, "addr %p len %zu", addr, len);

	pmem_deep_flush(addr, len);
	return pmem_deep_drain(addr, len);
}

/*
 * pmem_deep_drain -- perform deep drain on a memory range
 */
int
pmem_deep_drain(const void *addr, size_t len)
{
	LOG(3, "addr %p len %zu", addr, len);

	return os_range_deep_common((uintptr_t)addr, len);
}

#if VG_PMEMCHECK_ENABLED
/*
 * pmem_emit_log -- logs library and function names to pmemcheck store log
 */
void
pmem_emit_log(const char *func, int order)
{
	util_emit_log("libpmem", func, order);
}
#endif

#if FAULT_INJECTION
void
pmem_inject_fault_at(enum pmem_allocation_type type, int nth,
						const char *at)
{
	common_inject_fault_at(type, nth, at);
}

int
pmem_fault_injection_enabled(void)
{
	return common_fault_injection_enabled();
}
#endif
