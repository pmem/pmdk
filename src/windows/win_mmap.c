/*
 * Copyright 2015-2016, Intel Corporation
 * Copyright (c) Microsoft Corporation. All rights reserved.
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
 * win_mmap.c -- memory-mapped files for Windows
 */

/*
 * XXX - The initial approach to NVML for Windows port was to minimize the
 * amount of changes required in the core part of the library, and to avoid
 * preprocessor conditionals, if possible.  For that reason, some of the
 * Linux system calls that have no equivalents on Windows have been emulated
 * using Windows API.
 * Note that it was not a goal to fully emulate POSIX-compliant behavior
 * of mentioned functions.  They are used only internally, so current
 * implementation is just good enough to satisfy NVML needs and to make it
 * work on Windows.
 *
 * This is a subject for change in the future.  Likely, all these functions
 * will be replaced with "util_xxx" wrappers with OS-specific implementation
 * for Linux and Windows.
 *
 * Known issues:
 * - on Windows, mapping granularity/alignment is 64KB, not 4KB;
 * - mprotect() behavior and protection flag handling in mmap() is slightly
 *   different than on Linux (see comments below).
 */

#include <sys/mman.h>
#include <sys/queue.h>
#include "mmap.h"
#include "out.h"

#define roundup(x, y)	((((x) + ((y) - 1)) / (y)) * (y))

NTSTATUS NtFreeVirtualMemory(
  _In_    HANDLE  ProcessHandle,
  _Inout_ PVOID   *BaseAddress,
  _Inout_ PSIZE_T RegionSize,
  _In_    ULONG   FreeType
);

/* allocation/mmap granularity */
unsigned long long Mmap_align;

/* page size */
unsigned long long Pagesize;

/*
 * this structure tracks the file mappings outstanding per file handle
 */
typedef struct FILE_MAPPING_TRACKER {
	LIST_ENTRY(FILE_MAPPING_TRACKER) ListEntry;
	HANDLE FileHandle;
	HANDLE FileMappingHandle;
	PVOID *BaseAddress;
	PVOID *EndAddress;
	DWORD Access;
	off_t Offset;
} *PFILE_MAPPING_TRACKER;

HANDLE FileMappingListMutex = NULL;
LIST_HEAD(FMLHead, FILE_MAPPING_TRACKER) FileMappingListHead =
	LIST_HEAD_INITIALIZER(FileMappingListHead);

/*
 * mmap_init -- (internal) load-time initialization of file mapping tracker
 *
 * Called automatically by the run-time loader.
 */
static void
mmap_init(void)
{
	LIST_INIT(&FileMappingListHead);

	FileMappingListMutex = CreateMutex(NULL, FALSE, NULL);

	SYSTEM_INFO si;
	GetSystemInfo(&si);
	Mmap_align = si.dwAllocationGranularity;
	Pagesize = si.dwPageSize;
}

/*
 * mmap_fini -- (internal) file mapping tracker cleanup routine
 *
 * Called automatically when the process terminates.
 */
static void
mmap_fini(void)
{
	if (FileMappingListMutex == NULL)
		return;

	/*
	 * Let's make sure that no one is in the middle of updating the
	 * list by grabbing the lock.  There is still a race condition
	 * with someone coming in while we're tearing it down and trying
	 */
	WaitForSingleObject(FileMappingListMutex, INFINITE);
	ReleaseMutex(FileMappingListMutex);
	CloseHandle(FileMappingListMutex);

	while (!LIST_EMPTY(&FileMappingListHead)) {
		PFILE_MAPPING_TRACKER mt;
		mt = (PFILE_MAPPING_TRACKER)LIST_FIRST(&FileMappingListHead);

		LIST_REMOVE(mt, ListEntry);

		if (mt->BaseAddress != NULL)
			UnmapViewOfFile(mt->BaseAddress);

		if (mt->FileMappingHandle != NULL)
			CloseHandle(mt->FileMappingHandle);

		free(mt);
	}
}

#define PROT_ALL (PROT_READ|PROT_WRITE|PROT_EXEC)

/*
 * mfree_reservation -- frees the range that's previously reserved
 */
int
mfree_reservation(void *addr, size_t len)
{
	size_t bytes_returned;
	MEMORY_BASIC_INFORMATION basic_info;

	bytes_returned = VirtualQuery(addr, &basic_info, sizeof(basic_info));

	if (bytes_returned != sizeof(basic_info)) {
		ERR("cannot query the virtual address properties of the range "
			"- addr: %p, len: %d", addr, len);
		errno = EINVAL;
		return -1;
	}

	if (basic_info.State == MEM_RESERVE) {
		DWORD nt_status;
		void *release_addr = addr;
		size_t release_size = len;
		nt_status = NtFreeVirtualMemory(GetCurrentProcess(),
			&release_addr, &release_size, MEM_RELEASE);
		if (nt_status != 0) {
			ERR("cannot release the reserved virtual space - "
				"addr: %p, len: %d, nt_status: 0x%08x", addr,
				len, nt_status);
			errno = EINVAL;
			return -1;
		}
		ASSERTeq(release_addr, addr);
		ASSERTeq(release_size, len);
		LOG(3, "freed reservation - addr: %p, size: %d", release_addr,
			release_size);
	} else {
		LOG(4, "range not reserved - addr: %p, size: %d", addr, len);
	}

	return 0;
}

/*
 * mmap -- map file into memory
 *
 * XXX - If read-only mapping was created initially, it is not possible
 * to change protection to R/W, even if the file itself was open in R/W mode.
 * To workaround that, we could modify mmap() to create R/W mapping first,
 * then change the protection to R/O.  This way, it should be possible
 * to elevate permissions later.
 */
void *
mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset)
{
	if (len == 0) {
		errno = EINVAL;
		return MAP_FAILED;
	}

	if ((prot & ~PROT_ALL) != 0) {
		/* invalid protection flags */
		errno = EINVAL;
		return MAP_FAILED;
	}

	if (((flags & MAP_PRIVATE) && (flags & MAP_SHARED)) ||
	    ((flags & (MAP_PRIVATE | MAP_SHARED)) == 0)) {
		/* neither MAP_PRIVATE or MAP_SHARED is set, or both are set */
		errno = EINVAL;
		return MAP_FAILED;
	}

	DWORD protect = 0;
	DWORD access = 0;

	/* on x86, PROT_WRITE implies PROT_READ */
	if (prot & PROT_WRITE) {
		if (flags & MAP_PRIVATE) {
			access = FILE_MAP_COPY;
			if (prot & PROT_EXEC)
				protect = PAGE_EXECUTE_WRITECOPY;
			else
				protect = PAGE_WRITECOPY;
		} else {
			/* FILE_MAP_ALL_ACCESS == FILE_MAP_WRITE */
			access = FILE_MAP_ALL_ACCESS;
			if (prot & PROT_EXEC)
				protect = PAGE_EXECUTE_READWRITE;
			else
				protect = PAGE_READWRITE;
		}
	} else if (prot & PROT_READ) {
		access = FILE_MAP_READ;
		if (prot & PROT_EXEC)
			protect = PAGE_EXECUTE_READ;
		else
			protect = PAGE_READONLY;
	} else {
		/* XXX - PAGE_NOACCESS is not supported by CreateFileMapping */
		errno = ENOTSUP;
		return MAP_FAILED;
	}

	if (((uintptr_t)addr % Mmap_align) != 0) {
		if ((flags & MAP_FIXED) == 0) {
			/* ignore invalid hint if no MAP_FIXED flag is set */
			addr = NULL;
		} else {
			errno = EINVAL;
			return MAP_FAILED;
		}
	}

	if ((flags & MAP_FIXED) != 0) {
		/*
		 * free any reservations that the caller might have, also we
		 * have to unmap any existing mappings in this region as per
		 * mmap's manual.
		 * XXX - Ideally we should unmap only if the prot and flags
		 * are similar, we are deferring it as we don't rely on it
		 * yet.
		 */

		munmap(addr, len);
	}

	/* XXX - MAP_NORESERVE */

	HANDLE fh;
	if (flags & MAP_ANON) {
		/*
		 * in our implementation we are choosing to ignore fd when
		 * MAN_ANON is set, instead of failing.
		 */
		fh = INVALID_HANDLE_VALUE;

		/* ignore/override offset */
		offset = 0;
	} else {
		LARGE_INTEGER filesize;

		if (fd == -1) {
			errno = EBADF;
			return MAP_FAILED;
		}
		fh = (HANDLE)_get_osfhandle(fd);

		/*
		* if we are asked to map more than the file size, map till the file size
		* and reserve the following.
		*/

		if (!GetFileSizeEx(fh, &filesize)) {
			ERR("cannot query the file size - fh: %d, win32error: %d", fd, GetLastError());
			return MAP_FAILED;
		}

		if (offset > (off_t)filesize.QuadPart) {
			errno = EINVAL;
			ERR("offset is beyond the file size");
			return MAP_FAILED;
		}

		if ((offset + len) > (size_t)filesize.QuadPart) {
			/*
			 * reserve virtual address for the rest of range we need
			 * to map, and free a portion in the beginning for this
			 * allocation
			 */
			void *reserved_addr = VirtualAlloc(addr, len,
						MEM_RESERVE, PAGE_NOACCESS);
			if (reserved_addr == NULL) {
				ERR("cannot find a contiguous region - "
					"addr: %p, len: %lx, gle: 0x%08x", addr,
					len, GetLastError());
				errno = ENOMEM;
				return MAP_FAILED;
			}

			if (addr != NULL && addr != reserved_addr &&
				(flags & MAP_FIXED) != 0) {
				ERR("cannot find a contiguous region - "
					"addr: %p, len: %lx, gle: 0x%08x", addr,
					len, GetLastError());
				if (mfree_reservation(reserved_addr, 0) != 0) {
					ASSERT(FALSE);
					ERR("cannot free reserved region");
				}
				errno = ENOMEM;
				return MAP_FAILED;
			}

			addr = reserved_addr;
			len = (size_t)filesize.QuadPart - offset;
			if (mfree_reservation(reserved_addr, len) != 0) {
				ASSERT(FALSE);
				ERR("cannot free reserved region");
				return MAP_FAILED;
			}
		}
	}

	HANDLE fileMapping = CreateFileMapping(fh,
					NULL, /* security attributes */
					protect,
					(DWORD) ((len + offset) >> 32),
					(DWORD) ((len + offset) & 0xFFFFFFFF),
					NULL);

	if (fileMapping == NULL) {
		if (GetLastError() == ERROR_ACCESS_DENIED)
			errno = EACCES;
		else
			errno = EINVAL; /* XXX */
		return MAP_FAILED;
	}

	void *base = MapViewOfFileEx(fileMapping,
				access,
				(DWORD) (offset >> 32),
				(DWORD) (offset & 0xFFFFFFFF),
				len,
				addr); /* hint address */

	if (base == NULL) {
		if (addr == NULL || (flags & MAP_FIXED) != 0) {
			errno = EINVAL;
			CloseHandle(fileMapping);
			return MAP_FAILED;
		}

		/* try again w/o hint */
		base = MapViewOfFileEx(fileMapping,
				access,
				(DWORD) (offset >> 32),
				(DWORD) (offset & 0xFFFFFFFF),
				len,
				NULL); /* no hint address */
	}

	if (base == NULL) {
		CloseHandle(fileMapping);
		return MAP_FAILED;
	}

    /*
     * if we can't map at the address given by the caller, fail
     * the call if MAP_FIXED is set
     */
    if (addr != NULL && base != addr && (flags & MAP_FIXED)) {
		UnmapViewOfFile(addr);
		errno = EINVAL;
		CloseHandle(fileMapping);
		return MAP_FAILED;
    }

	/*
	 * We will track the file mapping handle on a lookaside list so that
	 * we don't have to modify the fact that we only return back the base
	 * address rather than a more elaborate structure.
	 */

	PFILE_MAPPING_TRACKER mt =
		malloc(sizeof(struct FILE_MAPPING_TRACKER));

	if (mt == NULL) {
		CloseHandle(fileMapping);
		return MAP_FAILED;
	}

	mt->FileHandle = fh;
	mt->FileMappingHandle = fileMapping;
	mt->BaseAddress = base;
	mt->EndAddress = (PVOID *)((char *)base + roundup(len, Mmap_align));
	mt->Access = access;
	mt->Offset = offset;

	WaitForSingleObject(FileMappingListMutex, INFINITE);
	LIST_INSERT_HEAD(&FileMappingListHead, mt, ListEntry);
	ReleaseMutex(FileMappingListMutex);

	return base;
}

/*
 * mmap_split -- (internal) replace existing mapping with another one(s)
 */
static int
mmap_split(PFILE_MAPPING_TRACKER mt, PVOID *begin, PVOID *end)
{
	PFILE_MAPPING_TRACKER mtb = NULL;
	PFILE_MAPPING_TRACKER mte = NULL;
	HANDLE fmh = mt->FileMappingHandle;

	if (begin > mt->BaseAddress) {
		/* new mapping at the beginning */
		mtb = malloc(sizeof(struct FILE_MAPPING_TRACKER));
		if (mtb == NULL)
			goto err;

		mtb->FileHandle = mt->FileHandle;
		mtb->FileMappingHandle = mt->FileMappingHandle;
		mtb->BaseAddress = mt->BaseAddress;
		mtb->EndAddress = begin;
		mtb->Access = mt->Access;
		mtb->Offset = mt->Offset;
	}

	if (end < mt->EndAddress) {
		/* new mapping at the end */
		mte = malloc(sizeof(struct FILE_MAPPING_TRACKER));
		if (mte == NULL)
			goto err;

		mte->FileHandle = mt->FileHandle;
		if (!mtb)
			mte->FileMappingHandle = mt->FileMappingHandle;
		else
			DuplicateHandle(GetCurrentProcess(), mt->FileMappingHandle,
				GetCurrentProcess(), &mte->FileMappingHandle,
				0, FALSE, DUPLICATE_SAME_ACCESS);
		mte->BaseAddress = end;
		mte->EndAddress = mt->EndAddress;
		mte->Access = mt->Access;
		mte->Offset = mt->Offset +
			((char *)mte->BaseAddress - (char *)mt->BaseAddress);
	}

	if (UnmapViewOfFile(mt->BaseAddress) == FALSE)
		goto err;

	if (!mtb && !mte)
		CloseHandle(fmh);

	LIST_REMOVE(mt, ListEntry);
	free(mt);

	if (mtb) {
		void *base = MapViewOfFileEx(mtb->FileMappingHandle,
			mtb->Access,
			(DWORD) (mtb->Offset >> 32),
			(DWORD) (mtb->Offset & 0xFFFFFFFF),
			(char *)mtb->EndAddress - (char *)mtb->BaseAddress,
			mtb->BaseAddress); /* hint address */

		if (base == NULL)
			goto err_close;

		LIST_INSERT_HEAD(&FileMappingListHead, mtb, ListEntry);
	}

	if (mte) {
		void *base = MapViewOfFileEx(mte->FileMappingHandle,
			mte->Access,
			(DWORD) (mte->Offset >> 32),
			(DWORD) (mte->Offset & 0xFFFFFFFF),
			(char *)mte->EndAddress - (char *)mte->BaseAddress,
			mte->BaseAddress); /* hint address */

		if (base == NULL)
			goto err_close;

		LIST_INSERT_HEAD(&FileMappingListHead, mte, ListEntry);
	}

	return 0;

err_close:
	/*
	 * Since the original mapping is already deleted, there's not much
	 * we can do...
	 */
	CloseHandle(fmh);

err:
	free(mtb);
	free(mte);
	return -1;
}

/*
 * munmap -- delete mapping
 */
int
munmap(void *addr, size_t len)
{
	if (((uintptr_t)addr % Mmap_align) != 0) {
		errno = EINVAL;
		return -1;
	}

	int retval = -1;

	len = roundup(len, Mmap_align);

	PVOID *begin = addr;
	PVOID *end = (PVOID *)((char *)addr + len);

	WaitForSingleObject(FileMappingListMutex, INFINITE);

	PFILE_MAPPING_TRACKER next;
	PFILE_MAPPING_TRACKER mt;
	mt = (PFILE_MAPPING_TRACKER)LIST_FIRST(&FileMappingListHead);
	while (len > 0 && mt != NULL) {
		if (begin >= mt->EndAddress || end <= mt->BaseAddress) {
			/* range not in the mapping */
			mt = (PFILE_MAPPING_TRACKER)LIST_NEXT(mt, ListEntry);
			continue;
		}

		PVOID *begin2 = begin > mt->BaseAddress ?
				begin : mt->BaseAddress;
		PVOID *end2 = end < mt->EndAddress ?
				end : mt->EndAddress;

		size_t len2 = (char *)end2 - (char *)begin2;

		next = (PFILE_MAPPING_TRACKER)LIST_NEXT(mt, ListEntry);

		if (mmap_split(mt, begin2, end2) != 0)
			goto err;

		len -= len2;
		mt = next;
	}

	/*
	 * if we didn't find any mapped regions in our list attempt to free
	 * if the entire range is reserved.
	 *
	 * XXX: we don't handle a range having few mapped regions and few
	 * reserved regions
	 */
	if (len > 0)
		mfree_reservation(addr, len);

	retval = 0;

err:
	ReleaseMutex(FileMappingListMutex);

	if (retval == -1)
		errno = EINVAL;

	return retval;
}

#define MS_ALL (MS_SYNC|MS_ASYNC|MS_INVALIDATE)

/*
 * msync -- synchronize a file with a memory map
 */
int
msync(void *addr, size_t len, int flags)
{
	if ((flags & ~MS_ALL) != 0) {
		errno = EINVAL;
		return -1;
	}

	/*
	 * XXX - On Linux it is allowed to call msync() without MS_SYNC
	 * nor MS_ASYNC.
	 */
	if (((flags & MS_SYNC) && (flags & MS_ASYNC)) ||
	    ((flags & (MS_SYNC | MS_ASYNC)) == 0)) {
		/* neither MS_SYNC or MS_ASYNC is set, or both are set */
		errno = EINVAL;
		return -1;
	}

	if (((uintptr_t)addr % Pagesize) != 0) {
		errno = EINVAL;
		return -1;
	}

	if (len == 0)
		return 0; /* do nothing */

	len = roundup(len, Pagesize);

	int retval = -1;

	PVOID *begin = addr;
	PVOID *end = (PVOID *)((char *)addr + len);

	WaitForSingleObject(FileMappingListMutex, INFINITE);

	PFILE_MAPPING_TRACKER mt;
	mt = (PFILE_MAPPING_TRACKER)LIST_FIRST(&FileMappingListHead);
	while (len > 0 && mt != NULL) {
		if (begin >= mt->EndAddress || end <= mt->BaseAddress) {
			/* range not in the mapping */
			mt = (PFILE_MAPPING_TRACKER)LIST_NEXT(mt, ListEntry);
			continue;
		}

		PVOID *begin2 = begin > mt->BaseAddress ?
			begin : mt->BaseAddress;
		PVOID *end2 = end < mt->EndAddress ?
			end : mt->EndAddress;

		size_t len2 = (char *)end2 - (char *)begin2;

		if (FlushViewOfFile(begin2, len2) == FALSE)
			goto err;

		if (FlushFileBuffers(mt->FileHandle) == FALSE)
			goto err;

		len -= len2;
		mt = (PFILE_MAPPING_TRACKER)LIST_NEXT(mt, ListEntry);
	}

	if (len > 0)
		errno = ENOMEM;
	else
		retval = 0;

err:
	ReleaseMutex(FileMappingListMutex);
	return retval;
}

#define PROT_ALL (PROT_READ|PROT_WRITE|PROT_EXEC)

/*
 * mprotect -- set protection on a region of memory
 *
 * XXX - If the memory range passed to mprotect() includes invalid pages,
 * returned status will indicate error, and errno is set to ENOMEM.
 * However, the protection change is actually applied to all the valid pages,
 * ingoring the rest.
 * This is different than on Linux, where it stops on the first invalid page.
 */
int
mprotect(void *addr, size_t len, int prot)
{

	if (((uintptr_t)addr % Pagesize) != 0) {
		errno = EINVAL;
		return -1;
	}

	if (len == 0)
		return 0; /* do nothing */

	len = roundup(len, Pagesize);

	DWORD protect = 0;

	if ((prot & PROT_READ) && (prot & PROT_WRITE)) {
		protect |= PAGE_READWRITE;
		if (prot & PROT_EXEC)
			protect |= PAGE_EXECUTE_READWRITE;
	} else if (prot & PROT_READ) {
		protect |= PAGE_READONLY;
		if (prot & PROT_EXEC)
			protect |= PAGE_EXECUTE_READ;
	} else {
		protect |= PAGE_NOACCESS;
	}

	int retval = -1;

	PVOID *begin = addr;
	PVOID *end = (PVOID *)((char *)addr + len);

	WaitForSingleObject(FileMappingListMutex, INFINITE);

	PFILE_MAPPING_TRACKER mt;
	mt = (PFILE_MAPPING_TRACKER)LIST_FIRST(&FileMappingListHead);
	while (len > 0 && mt != NULL) {
		if (begin >= mt->EndAddress || end <= mt->BaseAddress) {
			/* range not in the mapping */
			mt = (PFILE_MAPPING_TRACKER)LIST_NEXT(mt, ListEntry);
			continue;
		}

		PVOID *begin2 = begin > mt->BaseAddress ?
				begin : mt->BaseAddress;
		PVOID *end2 = end < mt->EndAddress ?
				end : mt->EndAddress;

		size_t len2 = (char *)end2 - (char *)begin2;

		DWORD oldprot = 0;
		BOOL ret;
		ret = VirtualProtect(begin2, len2, protect, &oldprot);
		if (ret == FALSE) {
			/* translate error code */
			switch (GetLastError()) {
				case ERROR_INVALID_PARAMETER:
					errno = EACCES;
					break;
				case ERROR_INVALID_ADDRESS:
					errno = ENOMEM;
					break;
				default:
					errno = EINVAL;
					break;
			}
			goto err;
		}

		len -= len2;
		mt = (PFILE_MAPPING_TRACKER)LIST_NEXT(mt, ListEntry);
	}

	if (len > 0)
		errno = ENOMEM;
	else
		retval = 0;

err:
	ReleaseMutex(FileMappingListMutex);
	return retval;
}

/*
 * library constructor/destructor functions
 */
MSVC_CONSTR(mmap_init)
MSVC_DESTR(mmap_fini)
