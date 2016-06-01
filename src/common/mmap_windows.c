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
#include <sys/queue.h>


/*
 * this structure tracks the file mappings outstanding per file handle
 */
typedef struct FILE_MAPPING_TRACKER {
	LIST_ENTRY(FILE_MAPPING_TRACKER) ListEntry;
	HANDLE FileHandle;
	HANDLE FileMappingHandle;
	PVOID *BaseAddress;
	PVOID *EndAddress;
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

		PFILE_MAPPING_TRACKER pMappingTracker =
			(PFILE_MAPPING_TRACKER)LIST_FIRST(&FileMappingListHead);
		LIST_REMOVE(pMappingTracker, ListEntry);

		if (pMappingTracker->BaseAddress != NULL)
			UnmapViewOfFile(pMappingTracker->BaseAddress);

		if (pMappingTracker->FileMappingHandle != NULL)
			CloseHandle(pMappingTracker->FileMappingHandle);

		free(pMappingTracker);

	}
}

/*
 * mmap -- map file into memory
 */
void *
mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset)
{
	DWORD protect = 0;

	if ((prot & PROT_READ) && (prot & PROT_WRITE)) {
		if (flags & MAP_PRIVATE) {
			if (prot & PROT_EXEC)
				protect = PAGE_EXECUTE_WRITECOPY;
			else
				protect = PAGE_WRITECOPY;
		} else {
			if (prot & PROT_EXEC)
				protect = PAGE_EXECUTE_READWRITE;
			else
				protect = PAGE_READWRITE;
		}
	} else if (prot & PROT_READ) {
		if (prot & PROT_EXEC)
			protect = PAGE_EXECUTE_READ;
		else
			protect = PAGE_READONLY;
	} else {
		/* XXX - PAGE_NOACCESS  */
		return MAP_FAILED;
	}

	/* XXX - MAP_NORESERVE */

	HANDLE fh = (HANDLE)_get_osfhandle(fd);

	HANDLE fileMapping = CreateFileMapping(fh,
					NULL, /* security attributes */
					protect,
					(DWORD) (len >> 32),
					(DWORD) (len & 0xFFFFFFFF),
					NULL);

	if (fileMapping == NULL)
		return MAP_FAILED;

	DWORD access;
	if (flags & MAP_PRIVATE)
		access = FILE_MAP_COPY;
	else
		access = FILE_MAP_ALL_ACCESS;

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

	PFILE_MAPPING_TRACKER pMappingTracker =
		malloc(sizeof(struct FILE_MAPPING_TRACKER));

	if (pMappingTracker == NULL) {
		CloseHandle(fileMapping);
		return MAP_FAILED;
	}

	pMappingTracker->FileHandle = fh;
	pMappingTracker->FileMappingHandle = fileMapping;
	pMappingTracker->BaseAddress = base;
	pMappingTracker->EndAddress = (PVOID *)base + len;

	WaitForSingleObject(FileMappingListMutex, INFINITE);
	LIST_INSERT_HEAD(&FileMappingListHead, pMappingTracker,
		ListEntry);
	ReleaseMutex(FileMappingListMutex);

	return base;
}

/*
 * munmap -- unmap file
 */
int
munmap(void *addr, size_t len)
{
	PFILE_MAPPING_TRACKER pMappingTracker = NULL;
	int retval = -1;
	BOOLEAN haveMutex = TRUE;

	WaitForSingleObject(FileMappingListMutex, INFINITE);

	pMappingTracker =
		(PFILE_MAPPING_TRACKER)LIST_FIRST(&FileMappingListHead);
	while (pMappingTracker != NULL) {

		if (pMappingTracker->BaseAddress == addr) {
			/*
			 * Let's release the list mutex before we do the work
			 * of unmapping and closing may take some time.
			 */

			LIST_REMOVE(pMappingTracker, ListEntry);
			ReleaseMutex(FileMappingListMutex);
			haveMutex = FALSE;

			if (UnmapViewOfFile(pMappingTracker->BaseAddress) != 0)
				retval = 0;

			CloseHandle(pMappingTracker->FileMappingHandle);
			free(pMappingTracker);

			break;
		}

		pMappingTracker =
			(PFILE_MAPPING_TRACKER)LIST_NEXT(pMappingTracker,
				ListEntry);
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

	PFILE_MAPPING_TRACKER pMappingTracker = NULL;
	int retval = -1;
	BOOLEAN haveMutex = TRUE;

	WaitForSingleObject(FileMappingListMutex, INFINITE);

	pMappingTracker =
		(PFILE_MAPPING_TRACKER)LIST_FIRST(&FileMappingListHead);
	while (pMappingTracker != NULL) {

		if (pMappingTracker->BaseAddress <= (PVOID *)addr &&
		    pMappingTracker->EndAddress >= (PVOID *)addr + len) {

			if (FlushFileBuffers(pMappingTracker->FileHandle) != 0)
				retval = 0;

			break;
		}

		pMappingTracker =
			(PFILE_MAPPING_TRACKER)LIST_NEXT(pMappingTracker,
				ListEntry);
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
