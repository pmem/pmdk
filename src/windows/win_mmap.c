// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2019, Intel Corporation */
/*
 * Copyright (c) 2015-2017, Microsoft Corporation. All rights reserved.
 * Copyright (c) 2016, Hewlett Packard Enterprise Development LP
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
 * XXX - The initial approach to PMDK for Windows port was to minimize the
 * amount of changes required in the core part of the library, and to avoid
 * preprocessor conditionals, if possible.  For that reason, some of the
 * Linux system calls that have no equivalents on Windows have been emulated
 * using Windows API.
 * Note that it was not a goal to fully emulate POSIX-compliant behavior
 * of mentioned functions.  They are used only internally, so current
 * implementation is just good enough to satisfy PMDK needs and to make it
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
#include "mmap.h"
#include "util.h"
#include "out.h"
#include "win_mmap.h"

/* uncomment for more debug information on mmap trackers */
/* #define MMAP_DEBUG_INFO */

NTSTATUS
NtFreeVirtualMemory(_In_ HANDLE ProcessHandle, _Inout_ PVOID *BaseAddress,
	_Inout_ PSIZE_T RegionSize, _In_ ULONG FreeType);

/*
 * XXX Unify the Linux and Windows code and replace this structure with
 * the map tracking list defined in mmap.h.
 */
SRWLOCK FileMappingQLock = SRWLOCK_INIT;
struct FMLHead FileMappingQHead =
	PMDK_SORTEDQ_HEAD_INITIALIZER(FileMappingQHead);

/*
 * mmap_file_mapping_comparer -- (internal) compares the two file mapping
 * trackers
 */
static LONG_PTR
mmap_file_mapping_comparer(PFILE_MAPPING_TRACKER a, PFILE_MAPPING_TRACKER b)
{
	return ((LONG_PTR)a->BaseAddress - (LONG_PTR)b->BaseAddress);
}

#ifdef MMAP_DEBUG_INFO
/*
 * mmap_info -- (internal) dump info about all the mapping trackers
 */
static void
mmap_info(void)
{
	LOG(4, NULL);

	AcquireSRWLockShared(&FileMappingQLock);

	PFILE_MAPPING_TRACKER mt;
	for (mt = PMDK_SORTEDQ_FIRST(&FileMappingQHead);
		mt != (void *)&FileMappingQHead;
		mt = PMDK_SORTEDQ_NEXT(mt, ListEntry)) {

		LOG(4, "FH %08x FMH %08x AD %p-%p (%zu) "
			"OF %08x FL %zu AC %d F %d",
			mt->FileHandle,
			mt->FileMappingHandle,
			mt->BaseAddress,
			mt->EndAddress,
			(char *)mt->EndAddress - (char *)mt->BaseAddress,
			mt->Offset,
			mt->FileLen,
			mt->Access,
			mt->Flags);
	}

	ReleaseSRWLockShared(&FileMappingQLock);
}
#endif

/*
 * mmap_reserve -- (internal) reserve virtual address range
 */
static void *
mmap_reserve(void *addr, size_t len)
{
	LOG(4, "addr %p len %zu", addr, len);

	ASSERTeq((uintptr_t)addr % Mmap_align, 0);
	ASSERTeq(len % Mmap_align, 0);

	void *reserved_addr = VirtualAlloc(addr, len,
				MEM_RESERVE, PAGE_NOACCESS);
	if (reserved_addr == NULL) {
		ERR("cannot find a contiguous region - "
			"addr: %p, len: %lx, gle: 0x%08x",
			addr, len, GetLastError());
		errno = ENOMEM;
		return MAP_FAILED;
	}

	return reserved_addr;
}

/*
 * mmap_unreserve -- (internal) frees the range that's previously reserved
 */
static int
mmap_unreserve(void *addr, size_t len)
{
	LOG(4, "addr %p len %zu", addr, len);

	ASSERTeq((uintptr_t)addr % Mmap_align, 0);
	ASSERTeq(len % Mmap_align, 0);

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
				"addr: %p, len: %d, nt_status: 0x%08x",
				addr, len, nt_status);
			errno = EINVAL;
			return -1;
		}
		ASSERTeq(release_addr, addr);
		ASSERTeq(release_size, len);
		LOG(4, "freed reservation - addr: %p, size: %d", release_addr,
			release_size);
	} else {
		LOG(4, "range not reserved - addr: %p, size: %d", addr, len);
	}

	return 0;
}

/*
 * win_mmap_init -- initialization of file mapping tracker
 */
void
win_mmap_init(void)
{
	AcquireSRWLockExclusive(&FileMappingQLock);
	PMDK_SORTEDQ_INIT(&FileMappingQHead);
	ReleaseSRWLockExclusive(&FileMappingQLock);
}

/*
 * win_mmap_fini -- file mapping tracker cleanup routine
 */
void
win_mmap_fini(void)
{
	/*
	 * Let's make sure that no one is in the middle of updating the
	 * list by grabbing the lock.
	 */
	AcquireSRWLockExclusive(&FileMappingQLock);

	while (!PMDK_SORTEDQ_EMPTY(&FileMappingQHead)) {
		PFILE_MAPPING_TRACKER mt;
		mt = (PFILE_MAPPING_TRACKER)PMDK_SORTEDQ_FIRST(
				&FileMappingQHead);

		PMDK_SORTEDQ_REMOVE(&FileMappingQHead, mt, ListEntry);

		if (mt->BaseAddress != NULL)
			UnmapViewOfFile(mt->BaseAddress);

		size_t release_size =
			(char *)mt->EndAddress - (char *)mt->BaseAddress;
		/*
		 * Free reservation after file mapping (if reservation was
		 * bigger than length of mapped file)
		 */
		void *release_addr = (char *)mt->BaseAddress + mt->FileLen;
		mmap_unreserve(release_addr, release_size - mt->FileLen);

		if (mt->FileMappingHandle != NULL)
			CloseHandle(mt->FileMappingHandle);

		if (mt->FileHandle != NULL)
			CloseHandle(mt->FileHandle);

		free(mt);
	}
	ReleaseSRWLockExclusive(&FileMappingQLock);
}

#define PROT_ALL (PROT_READ|PROT_WRITE|PROT_EXEC)

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
mmap(void *addr, size_t len, int prot, int flags, int fd, os_off_t offset)
{
	LOG(4, "addr %p len %zu prot %d flags %d fd %d offset %ju",
		addr, len, prot, flags, fd, offset);

	if (len == 0) {
		ERR("invalid length: %zu", len);
		errno = EINVAL;
		return MAP_FAILED;
	}

	if ((prot & ~PROT_ALL) != 0) {
		ERR("invalid flags: 0x%08x", flags);
		/* invalid protection flags */
		errno = EINVAL;
		return MAP_FAILED;
	}

	if (((flags & MAP_PRIVATE) && (flags & MAP_SHARED)) ||
	    ((flags & (MAP_PRIVATE | MAP_SHARED)) == 0)) {
		ERR("neither MAP_PRIVATE or MAP_SHARED is set, or both: 0x%08x",
			flags);
		errno = EINVAL;
		return MAP_FAILED;
	}

	/* XXX shall we use SEC_LARGE_PAGES flag? */
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
		ERR("PAGE_NOACCESS is not supported");
		errno = ENOTSUP;
		return MAP_FAILED;
	}

	if (((uintptr_t)addr % Mmap_align) != 0) {
		if ((flags & MAP_FIXED) == 0) {
			/* ignore invalid hint if no MAP_FIXED flag is set */
			addr = NULL;
		} else {
			ERR("hint address is not well-aligned: %p", addr);
			errno = EINVAL;
			return MAP_FAILED;
		}
	}

	if ((offset % Mmap_align) != 0) {
		ERR("offset is not well-aligned: %ju", offset);
		errno = EINVAL;
		return MAP_FAILED;
	}

	if ((flags & MAP_FIXED) != 0) {
		/*
		 * Free any reservations that the caller might have, also we
		 * have to unmap any existing mappings in this region as per
		 * mmap's manual.
		 * XXX - Ideally we should unmap only if the prot and flags
		 * are similar, we are deferring it as we don't rely on it
		 * yet.
		 */
		int ret = munmap(addr, len);
		if (ret != 0) {
			ERR("!munmap: addr %p len %zu", addr, len);
			return MAP_FAILED;
		}
	}

	size_t len_align = roundup(len, Mmap_align);
	size_t filelen;
	size_t filelen_align;
	HANDLE fh;
	if (flags & MAP_ANON) {
		/*
		 * In our implementation we are choosing to ignore fd when
		 * MAP_ANON is set, instead of failing.
		 */
		fh = INVALID_HANDLE_VALUE;

		/* ignore/override offset */
		offset = 0;
		filelen = len;
		filelen_align = len_align;

		if ((flags & MAP_NORESERVE) != 0) {
			/*
			 * For anonymous mappings the meaning of MAP_NORESERVE
			 * flag is pretty much the same as SEC_RESERVE.
			 */
			protect |= SEC_RESERVE;
		}
	} else {
		LARGE_INTEGER filesize;

		if (fd == -1) {
			ERR("invalid file descriptor: %d", fd);
			errno = EBADF;
			return MAP_FAILED;
		}

		/*
		 * We need to keep file handle open for proper
		 * implementation of msync() and to hold the file lock.
		 */
		if (!DuplicateHandle(GetCurrentProcess(),
				(HANDLE)_get_osfhandle(fd),
				GetCurrentProcess(), &fh,
				0, FALSE, DUPLICATE_SAME_ACCESS)) {
			ERR("cannot duplicate handle - fd: %d, gle: 0x%08x",
					fd, GetLastError());
			errno = ENOMEM;
			return MAP_FAILED;
		}

		/*
		 * If we are asked to map more than the file size, map till the
		 * file size and reserve the following.
		 */

		if (!GetFileSizeEx(fh, &filesize)) {
			ERR("cannot query the file size - fh: %d, gle: 0x%08x",
				fd, GetLastError());
			CloseHandle(fh);
			return MAP_FAILED;
		}

		if (offset >= (os_off_t)filesize.QuadPart) {
			errno = EINVAL;
			ERR("offset is beyond the file size");
			CloseHandle(fh);
			return MAP_FAILED;
		}

		/* calculate length of the mapped portion of the file */
		filelen = filesize.QuadPart - offset;
		if (filelen > len)
			filelen = len;
		filelen_align = roundup(filelen, Mmap_align);

		if ((offset + len) > (size_t)filesize.QuadPart) {
			/*
			 * Reserve virtual address for the rest of range we need
			 * to map, and free a portion in the beginning for this
			 * allocation.
			 */
			void *reserved_addr = mmap_reserve(addr, len_align);
			if (reserved_addr == MAP_FAILED) {
				ERR("cannot reserve region");
				CloseHandle(fh);
				return MAP_FAILED;
			}

			if (addr != reserved_addr && (flags & MAP_FIXED) != 0) {
				ERR("cannot find a contiguous region - "
					"addr: %p, len: %lx, gle: 0x%08x",
					addr, len, GetLastError());
				if (mmap_unreserve(reserved_addr,
						len_align) != 0) {
					ASSERT(FALSE);
					ERR("cannot free reserved region");
				}
				errno = ENOMEM;
				CloseHandle(fh);
				return MAP_FAILED;
			}

			addr = reserved_addr;
			if (mmap_unreserve(reserved_addr, filelen_align) != 0) {
				ASSERT(FALSE);
				ERR("cannot free reserved region");
				CloseHandle(fh);
				return MAP_FAILED;
			}
		}
	}

	HANDLE fmh = CreateFileMapping(fh,
			NULL, /* security attributes */
			protect,
			(DWORD) ((filelen + offset) >> 32),
			(DWORD) ((filelen + offset) & 0xFFFFFFFF),
			NULL);

	if (fmh == NULL) {
		DWORD gle = GetLastError();
		ERR("CreateFileMapping, gle: 0x%08x", gle);
		if (gle == ERROR_ACCESS_DENIED)
			errno = EACCES;
		else
			errno = EINVAL; /* XXX */
		CloseHandle(fh);
		return MAP_FAILED;
	}

	void *base = MapViewOfFileEx(fmh,
			access,
			(DWORD) (offset >> 32),
			(DWORD) (offset & 0xFFFFFFFF),
			filelen,
			addr); /* hint address */

	if (base == NULL) {
		if (addr == NULL || (flags & MAP_FIXED) != 0) {
			ERR("MapViewOfFileEx, gle: 0x%08x", GetLastError());
			errno = EINVAL;
			CloseHandle(fh);
			CloseHandle(fmh);
			return MAP_FAILED;
		}

		/* try again w/o hint */
		base = MapViewOfFileEx(fmh,
				access,
				(DWORD) (offset >> 32),
				(DWORD) (offset & 0xFFFFFFFF),
				filelen,
				NULL); /* no hint address */
	}

	if (base == NULL) {
		ERR("MapViewOfFileEx, gle: 0x%08x", GetLastError());
		errno = ENOMEM;
		CloseHandle(fh);
		CloseHandle(fmh);
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
		ERR("!malloc");
		CloseHandle(fh);
		CloseHandle(fmh);
		return MAP_FAILED;
	}

	mt->Flags = 0;
	mt->FileHandle = fh;
	mt->FileMappingHandle = fmh;
	mt->BaseAddress = base;
	mt->EndAddress = (void *)((char *)base + len_align);
	mt->Access = access;
	mt->Offset = offset;
	mt->FileLen = filelen_align;

	/*
	 * XXX: Use the QueryVirtualMemoryInformation when available in the new
	 * SDK.  If the file is DAX mapped say so in the FILE_MAPPING_TRACKER
	 * Flags.
	 */
	DWORD filesystemFlags;
	if (fh == INVALID_HANDLE_VALUE) {
		LOG(4, "anonymous mapping - not DAX mapped - handle: %p", fh);
	} else if (GetVolumeInformationByHandleW(fh, NULL, 0, NULL, NULL,
				&filesystemFlags, NULL, 0)) {
		if (filesystemFlags & FILE_DAX_VOLUME) {
			mt->Flags |= FILE_MAPPING_TRACKER_FLAG_DIRECT_MAPPED;
		} else {
			LOG(4, "file is not DAX mapped - handle: %p", fh);
		}
	} else {
		ERR("failed to query volume information : %08x",
			GetLastError());
	}

	AcquireSRWLockExclusive(&FileMappingQLock);

	PMDK_SORTEDQ_INSERT(&FileMappingQHead, mt, ListEntry,
			FILE_MAPPING_TRACKER, mmap_file_mapping_comparer);

	ReleaseSRWLockExclusive(&FileMappingQLock);

#ifdef MMAP_DEBUG_INFO
	mmap_info();
#endif

	return base;
}

/*
 * mmap_split -- (internal) replace existing mapping with another one(s)
 *
 * Unmaps the region between [begin,end].  If it's in a middle of the existing
 * mapping, it results in two new mappings and duplicated file/mapping handles.
 */
static int
mmap_split(PFILE_MAPPING_TRACKER mt, void *begin, void *end)
{
	LOG(4, "begin %p end %p", begin, end);

	ASSERTeq((uintptr_t)begin % Mmap_align, 0);
	ASSERTeq((uintptr_t)end % Mmap_align, 0);

	PFILE_MAPPING_TRACKER mtb = NULL;
	PFILE_MAPPING_TRACKER mte = NULL;
	HANDLE fh = mt->FileHandle;
	HANDLE fmh = mt->FileMappingHandle;
	size_t len;

	/*
	 * In this routine we copy flags from mt to the two subsets that we
	 * create.  All flags may not be appropriate to propagate so let's
	 * assert about the flags we know, if some one adds a new flag in the
	 * future they would know about this copy and take appropricate action.
	 */
	C_ASSERT(FILE_MAPPING_TRACKER_FLAGS_MASK == 1);

	/*
	 * 1)    b    e           b     e
	 *    xxxxxxxxxxxxx => xxx.......xxxx  -  mtb+mte
	 * 2)       b     e           b     e
	 *    xxxxxxxxxxxxx => xxxxxxx.......  -  mtb
	 * 3) b     e          b      e
	 *    xxxxxxxxxxxxx => ........xxxxxx  -  mte
	 * 4) b           e    b            e
	 *    xxxxxxxxxxxxx => ..............  -  <none>
	 */

	if (begin > mt->BaseAddress) {
		/* case #1/2 */
		/* new mapping at the beginning */
		mtb = malloc(sizeof(struct FILE_MAPPING_TRACKER));
		if (mtb == NULL) {
			ERR("!malloc");
			goto err;
		}

		mtb->Flags = mt->Flags;
		mtb->FileHandle = fh;
		mtb->FileMappingHandle = fmh;
		mtb->BaseAddress = mt->BaseAddress;
		mtb->EndAddress = begin;
		mtb->Access = mt->Access;
		mtb->Offset = mt->Offset;

		len = (char *)begin - (char *)mt->BaseAddress;
		mtb->FileLen = len >= mt->FileLen ? mt->FileLen : len;
	}

	if (end < mt->EndAddress) {
		/* case #1/3 */
		/* new mapping at the end */
		mte = malloc(sizeof(struct FILE_MAPPING_TRACKER));
		if (mte == NULL) {
			ERR("!malloc");
			goto err;
		}

		if (!mtb) {
			/* case #3 */
			mte->FileHandle = fh;
			mte->FileMappingHandle = fmh;
		} else {
			/* case #1 - need to duplicate handles */
			mte->FileHandle = NULL;
			mte->FileMappingHandle = NULL;

			if (!DuplicateHandle(GetCurrentProcess(), fh,
					GetCurrentProcess(),
					&mte->FileHandle,
					0, FALSE, DUPLICATE_SAME_ACCESS)) {
				ERR("DuplicateHandle, gle: 0x%08x",
					GetLastError());
				goto err;
			}

			if (!DuplicateHandle(GetCurrentProcess(), fmh,
					GetCurrentProcess(),
					&mte->FileMappingHandle,
					0, FALSE, DUPLICATE_SAME_ACCESS)) {
				ERR("DuplicateHandle, gle: 0x%08x",
					GetLastError());
				goto err;
			}
		}

		mte->Flags = mt->Flags;
		mte->BaseAddress = end;
		mte->EndAddress = mt->EndAddress;
		mte->Access = mt->Access;
		mte->Offset = mt->Offset +
			((char *)mte->BaseAddress - (char *)mt->BaseAddress);

		len = (char *)end - (char *)mt->BaseAddress;
		mte->FileLen = len >= mt->FileLen ? 0 : mt->FileLen - len;
	}

	if (mt->FileLen > 0 && UnmapViewOfFile(mt->BaseAddress) == FALSE) {
		ERR("UnmapViewOfFile, gle: 0x%08x", GetLastError());
		goto err;
	}

	len = (char *)mt->EndAddress - (char *)mt->BaseAddress;
	if (len > mt->FileLen) {
		void *addr = (char *)mt->BaseAddress + mt->FileLen;
		mmap_unreserve(addr, len - mt->FileLen);
	}

	if (!mtb && !mte) {
		/* case #4 */
		CloseHandle(fmh);
		CloseHandle(fh);
	}

	/*
	 * free entry for the original mapping
	 */
	PMDK_SORTEDQ_REMOVE(&FileMappingQHead, mt, ListEntry);
	free(mt);

	if (mtb) {
		len = (char *)mtb->EndAddress - (char *)mtb->BaseAddress;
		if (len > mtb->FileLen) {
			void *addr = (char *)mtb->BaseAddress + mtb->FileLen;
			void *raddr = mmap_reserve(addr, len - mtb->FileLen);
			if (raddr == MAP_FAILED) {
				ERR("cannot find a contiguous region - "
					"addr: %p, len: %lx, gle: 0x%08x",
					addr, len, GetLastError());
				goto err;
			}
		}

		if (mtb->FileLen > 0) {
			void *base = MapViewOfFileEx(mtb->FileMappingHandle,
				mtb->Access,
				(DWORD) (mtb->Offset >> 32),
				(DWORD) (mtb->Offset & 0xFFFFFFFF),
				mtb->FileLen,
				mtb->BaseAddress); /* hint address */

			if (base == NULL) {
				ERR("MapViewOfFileEx, gle: 0x%08x",
						GetLastError());
				goto err;
			}
		}

		PMDK_SORTEDQ_INSERT(&FileMappingQHead, mtb, ListEntry,
			FILE_MAPPING_TRACKER, mmap_file_mapping_comparer);
	}

	if (mte) {
		len = (char *)mte->EndAddress - (char *)mte->BaseAddress;
		if (len > mte->FileLen) {
			void *addr = (char *)mte->BaseAddress + mte->FileLen;
			void *raddr = mmap_reserve(addr, len - mte->FileLen);
			if (raddr == MAP_FAILED) {
				ERR("cannot find a contiguous region - "
					"addr: %p, len: %lx, gle: 0x%08x",
					addr, len, GetLastError());
				goto err;
			}
		}

		if (mte->FileLen > 0) {
			void *base = MapViewOfFileEx(mte->FileMappingHandle,
				mte->Access,
				(DWORD) (mte->Offset >> 32),
				(DWORD) (mte->Offset & 0xFFFFFFFF),
				mte->FileLen,
				mte->BaseAddress); /* hint address */

			if (base == NULL) {
				ERR("MapViewOfFileEx, gle: 0x%08x",
						GetLastError());
				goto err_mte;
			}
		}

		PMDK_SORTEDQ_INSERT(&FileMappingQHead, mte, ListEntry,
			FILE_MAPPING_TRACKER, mmap_file_mapping_comparer);
	}

	return 0;

err:
	if (mtb) {
		ASSERTeq(mtb->FileMappingHandle, fmh);
		ASSERTeq(mtb->FileHandle, fh);
		CloseHandle(mtb->FileMappingHandle);
		CloseHandle(mtb->FileHandle);

		len = (char *)mtb->EndAddress - (char *)mtb->BaseAddress;
		if (len > mtb->FileLen) {
			void *addr = (char *)mtb->BaseAddress + mtb->FileLen;
			mmap_unreserve(addr, len - mtb->FileLen);
		}
	}

err_mte:
	if (mte) {
		if (mte->FileMappingHandle)
			CloseHandle(mte->FileMappingHandle);
		if (mte->FileHandle)
			CloseHandle(mte->FileHandle);

		len = (char *)mte->EndAddress - (char *)mte->BaseAddress;
		if (len > mte->FileLen) {
			void *addr = (char *)mte->BaseAddress + mte->FileLen;
			mmap_unreserve(addr, len - mte->FileLen);
		}
	}

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
	LOG(4, "addr %p len %zu", addr, len);

	if (((uintptr_t)addr % Mmap_align) != 0) {
		ERR("address is not well-aligned: %p", addr);
		errno = EINVAL;
		return -1;
	}

	if (len == 0) {
		ERR("invalid length: %zu", len);
		errno = EINVAL;
		return -1;
	}

	int retval = -1;

	if (len > UINTPTR_MAX - (uintptr_t)addr) {
		/* limit len to not get beyond address space */
		len = UINTPTR_MAX - (uintptr_t)addr;
	}

	void *begin = addr;
	void *end = (void *)((char *)addr + len);

	AcquireSRWLockExclusive(&FileMappingQLock);

	PFILE_MAPPING_TRACKER mt;
	PFILE_MAPPING_TRACKER next;
	for (mt = PMDK_SORTEDQ_FIRST(&FileMappingQHead);
		mt != (void *)&FileMappingQHead;
		mt = next) {

		/*
		 * Pick the next entry before we split there by delete the
		 * this one (NOTE: mmap_spilt could delete this entry).
		 */
		next = PMDK_SORTEDQ_NEXT(mt, ListEntry);

		if (mt->BaseAddress >= end) {
			LOG(4, "ignoring all mapped ranges beyond given range");
			break;
		}

		if (mt->EndAddress <= begin) {
			LOG(4, "skipping a mapped range before given range");
			continue;
		}

		void *begin2 = begin > mt->BaseAddress ?
				begin : mt->BaseAddress;
		void *end2 = end < mt->EndAddress ?
				end : mt->EndAddress;

		size_t len2 = (char *)end2 - (char *)begin2;

		void *align_end = (void *)roundup((uintptr_t)end2, Mmap_align);
		if (mmap_split(mt, begin2, align_end) != 0) {
			LOG(2, "mapping split failed");
			goto err;
		}

		if (len > len2) {
			len -= len2;
		} else {
			len = 0;
			break;
		}
	}

	/*
	 * If we didn't find any mapped regions in our list attempt to free
	 * as if the entire range is reserved.
	 *
	 * XXX: We don't handle a range having few mapped regions and few
	 * reserved regions.
	 */
	if (len > 0)
		mmap_unreserve(addr, roundup(len, Mmap_align));

	retval = 0;

err:
	ReleaseSRWLockExclusive(&FileMappingQLock);

	if (retval == -1)
		errno = EINVAL;

#ifdef MMAP_DEBUG_INFO
	mmap_info();
#endif

	return retval;
}

#define MS_ALL (MS_SYNC|MS_ASYNC|MS_INVALIDATE)

/*
 * msync -- synchronize a file with a memory map
 */
int
msync(void *addr, size_t len, int flags)
{
	LOG(4, "addr %p len %zu flags %d", addr, len, flags);

	if ((flags & ~MS_ALL) != 0) {
		ERR("invalid flags: 0x%08x", flags);
		errno = EINVAL;
		return -1;
	}

	/*
	 * XXX - On Linux it is allowed to call msync() without MS_SYNC
	 * nor MS_ASYNC.
	 */
	if (((flags & MS_SYNC) && (flags & MS_ASYNC)) ||
	    ((flags & (MS_SYNC | MS_ASYNC)) == 0)) {
		ERR("neither MS_SYNC or MS_ASYNC is set, or both: 0x%08x",
			flags);
		errno = EINVAL;
		return -1;
	}

	if (((uintptr_t)addr % Pagesize) != 0) {
		ERR("address is not page-aligned: %p", addr);
		errno = EINVAL;
		return -1;
	}

	if (len == 0) {
		LOG(4, "zero-length region - do nothing");
		return 0; /* do nothing */
	}

	if (len > UINTPTR_MAX - (uintptr_t)addr) {
		/* limit len to not get beyond address space */
		len = UINTPTR_MAX - (uintptr_t)addr;
	}

	int retval = -1;

	void *begin = addr;
	void *end = (void *)((char *)addr + len);

	AcquireSRWLockShared(&FileMappingQLock);

	PFILE_MAPPING_TRACKER mt;
	PMDK_SORTEDQ_FOREACH(mt, &FileMappingQHead, ListEntry) {
		if (mt->BaseAddress >= end) {
			LOG(4, "ignoring all mapped ranges beyond given range");
			break;
		}
		if (mt->EndAddress <= begin) {
			LOG(4, "skipping a mapped range before given range");
			continue;
		}

		void *begin2 = begin > mt->BaseAddress ?
			begin : mt->BaseAddress;
		void *end2 = end < mt->EndAddress ?
			end : mt->EndAddress;

		size_t len2 = (char *)end2 - (char *)begin2;

		/* do nothing for anonymous mappings */
		if (mt->FileHandle != INVALID_HANDLE_VALUE) {
			if (FlushViewOfFile(begin2, len2) == FALSE) {
				ERR("FlushViewOfFile, gle: 0x%08x",
					GetLastError());
				errno = ENOMEM;
				goto err;
			}

			if (FlushFileBuffers(mt->FileHandle) == FALSE) {
				ERR("FlushFileBuffers, gle: 0x%08x",
					GetLastError());
				errno = EINVAL;
				goto err;
			}
		}

		if (len > len2) {
			len -= len2;
		} else {
			len = 0;
			break;
		}
	}

	if (len > 0) {
		ERR("indicated memory (or part of it) was not mapped");
		errno = ENOMEM;
	} else {
		retval = 0;
	}

err:
	ReleaseSRWLockShared(&FileMappingQLock);
	return retval;
}

#define PROT_ALL (PROT_READ|PROT_WRITE|PROT_EXEC)

/*
 * mprotect -- set protection on a region of memory
 *
 * XXX - If the memory range passed to mprotect() includes invalid pages,
 * returned status will indicate error, and errno is set to ENOMEM.
 * However, the protection change is actually applied to all the valid pages,
 * ignoring the rest.
 * This is different than on Linux, where it stops on the first invalid page.
 */
int
mprotect(void *addr, size_t len, int prot)
{
	LOG(4, "addr %p len %zu prot %d", addr, len, prot);

	if (((uintptr_t)addr % Pagesize) != 0) {
		ERR("address is not page-aligned: %p", addr);
		errno = EINVAL;
		return -1;
	}

	if (len == 0) {
		LOG(4, "zero-length region - do nothing");
		return 0; /* do nothing */
	}

	if (len > UINTPTR_MAX - (uintptr_t)addr) {
		len = UINTPTR_MAX - (uintptr_t)addr;
		LOG(4, "limit len to %zu to not get beyond address space", len);
	}

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

	void *begin = addr;
	void *end = (void *)((char *)addr + len);

	AcquireSRWLockShared(&FileMappingQLock);

	PFILE_MAPPING_TRACKER mt;
	PMDK_SORTEDQ_FOREACH(mt, &FileMappingQHead, ListEntry) {
		if (mt->BaseAddress >= end) {
			LOG(4, "ignoring all mapped ranges beyond given range");
			break;
		}
		if (mt->EndAddress <= begin) {
			LOG(4, "skipping a mapped range before given range");
			continue;
		}

		void *begin2 = begin > mt->BaseAddress ?
				begin : mt->BaseAddress;
		void *end2 = end < mt->EndAddress ?
				end : mt->EndAddress;

		/*
		 * protect of region to VirtualProtection must be compatible
		 * with the access protection specified for this region
		 * when the view was mapped using MapViewOfFileEx
		 */
		if (mt->Access == FILE_MAP_COPY) {
			if (protect & PAGE_READWRITE) {
				protect &= ~PAGE_READWRITE;
				protect |= PAGE_WRITECOPY;
			} else if (protect & PAGE_EXECUTE_READWRITE) {
				protect &= ~PAGE_EXECUTE_READWRITE;
				protect |= PAGE_EXECUTE_WRITECOPY;
			}
		}

		size_t len2 = (char *)end2 - (char *)begin2;

		DWORD oldprot = 0;
		BOOL ret;
		ret = VirtualProtect(begin2, len2, protect, &oldprot);
		if (ret == FALSE) {
			DWORD gle = GetLastError();
			ERR("VirtualProtect, gle: 0x%08x", gle);
			/* translate error code */
			switch (gle) {
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

		if (len > len2) {
			len -= len2;
		} else {
			len = 0;
			break;
		}
	}

	if (len > 0) {
		ERR("indicated memory (or part of it) was not mapped");
		errno = ENOMEM;
	} else {
		retval = 0;
	}

err:
	ReleaseSRWLockShared(&FileMappingQLock);
	return retval;
}
