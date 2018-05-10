/*
 * Copyright 2018, Intel Corporation
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
 * os_dimm_windows.c -- implementation of DIMMs API based on winapi
 */

#include "out.h"
#include "os.h"
#include "os_dimm.h"
#include "util.h"

#define GUID_SIZE sizeof("XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX")
#define UNC_PREFIX "\\\\?\\"
#define UNC_PREFIX_LEN (sizeof(UNC_PREFIX) - 1 /* \0 */)
#define VOLUME_PREFIX "\\\\.\\"
#define VOLUME_PREFIX_LEN (sizeof(VOLUME_PREFIX) - 1 /* \0 */)

/*
 * os_dimm_volume -- returns volume handle
 */
static HANDLE
os_dimm_volume_handle(const char *path)
{
	LOG(3, "path %s", path);
	char vol[MAX_PATH] = VOLUME_PREFIX;
	char *v = vol + VOLUME_PREFIX_LEN;

	if (strncmp(path, UNC_PREFIX, UNC_PREFIX_LEN) == 0) {
		path += UNC_PREFIX_LEN;
	}
	if (!GetVolumePathName(path, v, MAX_PATH - VOLUME_PREFIX_LEN)) {
		return INVALID_HANDLE_VALUE;
	}

	vol[strlen(vol) - 1] = '\0'; /* remove trailing \\ */
	return CreateFileA(vol, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL, OPEN_EXISTING, 0,  NULL);
}

/*
 * os_dimm_uid -- returns a file uid based on dimm guid
 */
int
os_dimm_uid(const char *path, char *uid, size_t *len)
{
	LOG(3, "path %s, uid %p, len %lu", path, uid, *len);
	if (uid == NULL) {
		*len = GUID_SIZE;
	} else {
		ASSERT(*len >= GUID_SIZE);
		HANDLE vHandle = os_dimm_volume_handle(path);
		if (vHandle == INVALID_HANDLE_VALUE)
			return -1;

		STORAGE_DEVICE_NUMBER_EX sdn;
		sdn.DeviceNumber = -1;
		DWORD dwBytesReturned = 0;
		if (!DeviceIoControl(vHandle,
				IOCTL_STORAGE_GET_DEVICE_NUMBER_EX,
				NULL, 0,
				&sdn, sizeof(sdn),
				&dwBytesReturned, NULL)) {
			/*
			 * IOCTL_STORAGE_GET_DEVICE_NUMBER_EX is not supported
			 * on this server
			 */
			memset(uid, 0, *len);
			CloseHandle(vHandle);
			return 0;
		}
		GUID guid = sdn.DeviceGuid;
		snprintf(uid, GUID_SIZE,
			"%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",
			guid.Data1, guid.Data2, guid.Data3, guid.Data4[0],
			guid.Data4[1], guid.Data4[2], guid.Data4[3],
			guid.Data4[4], guid.Data4[5], guid.Data4[6],
			guid.Data4[7]);
		CloseHandle(vHandle);
	}
	return 0;
}

/*
 * os_dimm_usc -- returns unsafe shutdown count
 */
int
os_dimm_usc(const char *path, uint64_t *usc)
{
	LOG(3, "path %s, usc %p", path, usc);
	*usc = 0;
	HANDLE vHandle = os_dimm_volume_handle(path);
	if (vHandle == INVALID_HANDLE_VALUE)
		return -1;

	STORAGE_PROPERTY_QUERY prop;
	DWORD dwSize;
	prop.PropertyId = StorageDeviceUnsafeShutdownCount;
	prop.QueryType = PropertyExistsQuery;
	STORAGE_DEVICE_UNSAFE_SHUTDOWN_COUNT ret;

	BOOL bResult = DeviceIoControl(vHandle,
		IOCTL_STORAGE_QUERY_PROPERTY,
		&prop, sizeof(prop),
		&ret, sizeof(ret),
		(LPDWORD)&dwSize, (LPOVERLAPPED)NULL);

	if (!bResult) {
		CloseHandle(vHandle);
		return 0; /* STORAGE_PROPERTY_QUERY not supported */
	}
	prop.QueryType = PropertyStandardQuery;
	bResult = DeviceIoControl(vHandle,
		IOCTL_STORAGE_QUERY_PROPERTY,
		&prop, sizeof(prop),
		&ret, sizeof(ret),
		(LPDWORD)&dwSize, (LPOVERLAPPED)NULL);

	CloseHandle(vHandle);
	if (!bResult)
		return -1;

	*usc = ret.UnsafeShutdownCount;

	return 0;
}

/*
 * os_dimm_files_namespace_badblocks -- fake os_dimm_files_namespace_badblocks()
 */
int
os_dimm_files_namespace_badblocks(const char *path, struct badblocks *bbs)
{
	LOG(3, "path %s", path);

	os_stat_t st;

	if (os_stat(path, &st)) {
		ERR("!stat %s", path);
		return -1;
	}

	return 0;
}

/*
 * os_dimm_devdax_clear_get_badblocks -- fake bad block getting
 *                                       and clearing routine
 */
int
os_dimm_devdax_clear_get_badblocks(const char *path, struct badblocks *bbs)
{
	LOG(3, "path %s badblocks %p", path, bbs);

	return 0;
}

/*
 * os_dimm_devdax_clear_badblocks -- fake bad block clearing routine
 */
int
os_dimm_devdax_clear_badblocks(const char *path)
{
	LOG(3, "path %s", path);

	return 0;
}
