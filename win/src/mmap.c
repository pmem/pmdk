/*
 * Copyright (c) 2015, Intel Corporation
 * Copyright (c) 2015, Microsoft Corporation
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

#include <sys/mman.h>

//#include "util.h"
//#include "out.h"


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


void
mmap_init(void)
{
	//LOG(3, NULL);

	InitializeListHead(&FileMappingListHead);

	FileMappingListMutex = CreateMutex(NULL, FALSE, NULL);
}


void
mmap_fini(void)
{
	//LOG(3, NULL);

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
mmap(void *addr, size_t len, int prot, int flags, int fd, size_t offset)
{
	//LOG(3, "addr %p len %zu prot %d flags %d fd %d offset %ju",
	//	addr, len, prot, flags, fd, offset);

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
		//ERR("file mapping with NOACCESS is not supported");
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

	if (fileMapping == NULL) {
		//ERR("!CreateFileMapping %zu bytes", len);
		return MAP_FAILED;
	}

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
		//ERR("!MapViewOfFile %zu bytes", len);
		return MAP_FAILED;
	}

	/*
	 * We will track the file mapping handle on a lookaside list so that
	 * we don't have to modify the fact that we only return back the base
	 * address rather than a more elaborate structure.
	 */

	PFILE_MAPPING_TRACKER mappingTracker =
			malloc(sizeof (FILE_MAPPING_TRACKER));

	if (mappingTracker == NULL) {
		CloseHandle(fileMapping);
		//ERR("!Malloc");
		return MAP_FAILED;
	}

	mappingTracker->FileHandle = fh;
	mappingTracker->FileMappingHandle = fileMapping;
	mappingTracker->BaseAddress = base;
	mappingTracker->EndAddress = (PVOID *)base + len;

	WaitForSingleObject(FileMappingListMutex, INFINITE);
	InsertHeadList(&FileMappingListHead, &mappingTracker->ListEntry);
	ReleaseMutex(FileMappingListMutex);

	//LOG(3, "mapped at %p", base);

	return base;
}

/*
 * munmap -- unmap file
 */
int
munmap(void *addr, size_t len)
{
	//LOG(3, "addr %p len %zu", addr, len);

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

			if (UnmapViewOfFile(mappingTracker->BaseAddress) == 0)
				//ERR("!UnmapViewOfFile %p",
				//	mappingTracker->BaseAddress);
				;
			else
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
 * munmap -- synchronize a file with a memory map
 */
int
msync(void *addr, size_t len, int flags)
{
	//LOG(3, "addr %p len %zu flags %d", addr, len, flags);

	if (FlushViewOfFile(addr, len) == 0) {
		//ERR("!FlushViewOfFile: addr %p len %zu", addr, len);
		return -1;
	}

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

			if (FlushFileBuffers(
					mappingTracker->FileHandle) == 0)
				//ERR("!FlushFileBuffers %d",
				//	mappingTracker->FileHandle); /* XXX */
				;
			else
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
	//LOG(3, "addr %p len %zu prot %d", addr, len, prot);

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
		//ERR("!VirtualProtect: addr %p len %zu, prot %d",
		//	addr, len, prot);
		return -1;
	}

	return 0;
}
