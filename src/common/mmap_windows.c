/*
 * Copyright 2015-2016, Intel Corporation
 * Copyright 2015, Microsoft Corporation
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
 * mmap_windows.c -- memory-mapped files for Windows
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
 */

#include <sys/mman.h>

/*
 * this structure tracks the file mappings outstanding per file handle
 */
LIST_ENTRY FileMappingListHead;

typedef struct _FILE_MAPPING_TRACKER {
	LIST_ENTRY ListEntry;
	HANDLE FileHandle;
	HANDLE FileMappingHandle;
	PVOID *BaseAddress;
	PVOID *EndAddress;
} FILE_MAPPING_TRACKER, *PFILE_MAPPING_TRACKER;

HANDLE FileMappingListMutex = NULL;

/*
 * mmap_init -- (internal) load-time initialization of file mapping tracker
 *
 * Called automatically by the run-time loader.
 */
static void
mmap_init(void)
{
	InitializeListHead(&FileMappingListHead);
	FileMappingListMutex = CreateMutex(NULL, FALSE, NULL);
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

	while (!IsListEmpty(&FileMappingListHead)) {
		PLIST_ENTRY listEntry =
			RemoveTailList(&FileMappingListHead);

		PFILE_MAPPING_TRACKER mappingTracker =
			CONTAINING_RECORD(listEntry,
				FILE_MAPPING_TRACKER, ListEntry);

		if (mappingTracker->BaseAddress != NULL)
			UnmapViewOfFile(mappingTracker->BaseAddress);

		if (mappingTracker->FileMappingHandle != NULL)
			CloseHandle(mappingTracker->FileMappingHandle);

		free(mappingTracker);
	}
}

/*
 * mmap -- map file into memory
 */
void *
mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset)
{
	DWORD protect = 0;
	DWORD access = 0;

	if (len == 0) {
		errno = EINVAL;
		return MAP_FAILED;
	}

	if ((prot & ~(PROT_READ|PROT_WRITE|PROT_EXEC)) != 0) {
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
		/* XXX - PAGE_NOACCESS */
		errno = ENOTSUP;
		return MAP_FAILED;
	}

	/* XXX - MAP_NORESERVE */

	HANDLE fh;
	if (flags & MAP_ANON) {
		/* require 'fd' to be '-1' */
		if (fd != -1) {
			errno = EINVAL;
			return MAP_FAILED;
		}
		fh = INVALID_HANDLE_VALUE;

		/* ignore/override offset */
		offset = 0;
	} else {
		fh = (HANDLE)_get_osfhandle(fd);
	}

	HANDLE fileMapping = CreateFileMapping(fh,
					NULL, /* security attributes */
					protect,
					(DWORD) (len >> 32),
					(DWORD) (len & 0xFFFFFFFF),
					NULL);

	if (fileMapping == NULL)
		return MAP_FAILED;

	void *base = MapViewOfFile(fileMapping,
				access,
				(DWORD) (offset >> 32),
				(DWORD) (offset & 0xFFFFFFFF),
				len);

	if (base == NULL) {
		CloseHandle(fileMapping);
		return MAP_FAILED;
	}

	/*
	 * We will track the file mapping handle on a lookaside list so that
	 * we don't have to modify the fact that we only return back the base
	 * address rather than a more elaborate structure.
	 */

	PFILE_MAPPING_TRACKER mappingTracker =
			malloc(sizeof(FILE_MAPPING_TRACKER));

	if (mappingTracker == NULL) {
		CloseHandle(fileMapping);
		return MAP_FAILED;
	}

	mappingTracker->FileHandle = fh;
	mappingTracker->FileMappingHandle = fileMapping;
	mappingTracker->BaseAddress = base;
	mappingTracker->EndAddress = (PVOID *)base + len;

	WaitForSingleObject(FileMappingListMutex, INFINITE);
	InsertHeadList(&FileMappingListHead, &mappingTracker->ListEntry);
	ReleaseMutex(FileMappingListMutex);

	return base;
}

/*
 * munmap -- unmap file
 */
int
munmap(void *addr, size_t len)
{
	PLIST_ENTRY listEntry;
	int retval = -1;
	BOOLEAN haveMutex = TRUE;

	WaitForSingleObject(FileMappingListMutex, INFINITE);
	listEntry = FileMappingListHead.Flink;

	while (listEntry != &FileMappingListHead) {
		PFILE_MAPPING_TRACKER mappingTracker =
			CONTAINING_RECORD(listEntry, FILE_MAPPING_TRACKER,
						ListEntry);

		if (mappingTracker->BaseAddress == addr) {
			/*
			 * Let's release the list mutex before we do the work
			 * of unmapping and closing may take some time.
			 */

			RemoveEntryList(listEntry);
			ReleaseMutex(FileMappingListMutex);
			haveMutex = FALSE;

			if (UnmapViewOfFile(mappingTracker->BaseAddress) != 0)
				retval = 0;

			CloseHandle(mappingTracker->FileMappingHandle);
			free(mappingTracker);

			break;
		}

		listEntry = listEntry->Flink;
	}

	if (haveMutex)
		ReleaseMutex(FileMappingListMutex);

	if (retval == -1)
		errno = EINVAL;

	return retval;
}

/*
 * msync -- synchronize a file with a memory map
 */
int
msync(void *addr, size_t len, int flags)
{
	if (FlushViewOfFile(addr, len) == 0)
		return -1;

	PLIST_ENTRY listEntry;
	int retval = -1;
	BOOLEAN haveMutex = TRUE;

	WaitForSingleObject(FileMappingListMutex, INFINITE);
	listEntry = FileMappingListHead.Flink;

	while (listEntry != &FileMappingListHead) {
		PFILE_MAPPING_TRACKER mappingTracker =
			CONTAINING_RECORD(listEntry, FILE_MAPPING_TRACKER,
				ListEntry);

		if (mappingTracker->BaseAddress <= (PVOID *)addr &&
		    mappingTracker->EndAddress >= (PVOID *)addr + len) {

			if (FlushFileBuffers(mappingTracker->FileHandle) != 0)
				retval = 0;

			break;
		}

		listEntry = listEntry->Flink;
	}

	if (haveMutex)
		ReleaseMutex(FileMappingListMutex);

	return retval;
}

/*
 * mprotect -- set protection on a region of memory
 */
int
mprotect(void *addr, size_t len, int prot)
{
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

	DWORD oldprot;
	if (VirtualProtect(addr, len, protect, &oldprot) == 0) {
		return -1;
	}

	return 0;
}

/*
 * library constructor/destructor functions
 */
MSVC_CONSTR(mmap_init)
MSVC_DESTR(mmap_fini)
