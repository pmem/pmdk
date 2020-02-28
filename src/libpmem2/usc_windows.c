// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * usc_windows.c -- pmem2 usc function for windows
 */

#include "alloc.h"
#include "source.h"
#include "out.h"
#include "libpmem2.h"
#include "pmem2_utils.h"
#define GUID_SIZE sizeof("XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX")
#define VOLUME_PATH_SIZE sizeof("\\\\?\\Volume{}") + (GUID_SIZE - 2 /* \0 */)

/*
 * get_volume_handle -- returns volume handle
 */
static int
get_volume_handle(HANDLE handle, HANDLE *volume_handle)
{
	wchar_t *volume;
	wchar_t tmp[10];
	DWORD len =
		GetFinalPathNameByHandleW(handle, tmp, 10, VOLUME_NAME_GUID);

	if (len == 0) {
		ERR("!!GetFinalPathNameByHandleW");
		return pmem2_lasterror_to_err();
	}

	len *= sizeof(wchar_t);

	int err;
	volume = pmem2_malloc(len, &err);
	if (volume == NULL)
		return err;

	if (!GetFinalPathNameByHandleW(handle, volume, len,
		VOLUME_NAME_GUID)) {
		Free(volume);
		ERR("!!GetFinalPathNameByHandleW");
		return pmem2_lasterror_to_err();
	}

	ASSERTeq(volume[VOLUME_PATH_SIZE], '\\');
	volume[VOLUME_PATH_SIZE] = '\0';

	*volume_handle = CreateFileW(volume, /* path to the file */
		/* request access to send ioctl to the file */
		FILE_READ_ATTRIBUTES,
		/* do not block access to the file */
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL, /* security attributes */
		OPEN_EXISTING, /* open only if it exists */
		FILE_ATTRIBUTE_NORMAL, /* no attributes */
		NULL); /* used only for new files */

	Free(volume);
	if (*volume_handle == INVALID_HANDLE_VALUE) {
		ERR("!!CreateFileW");
		return pmem2_lasterror_to_err();
	}

	return 0;
}

static int
get_device_guid(HANDLE handle, GUID *guid)
{
	HANDLE vHandle;
	int ret = get_volume_handle(handle, &vHandle);
	if (vHandle == INVALID_HANDLE_VALUE)
		return ret;

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
		ERR(
			"Getting device id (IOCTL_STORAGE_GET_DEVICE_NUMBER_EX) is not supported on this system");
		CloseHandle(vHandle);
		return PMEM2_E_NOSUPP;
	}
	*guid = sdn.DeviceGuid;

	CloseHandle(vHandle);
	return 0;
}

int
pmem2_source_device_idW(const struct pmem2_source *src, wchar_t *id,
	size_t *len)
{
	if (id == NULL) {
		*len = GUID_SIZE * sizeof(*id);
		return 0;
	}

	if (*len < GUID_SIZE * sizeof(*id)) {
		ERR("id buffer is to small");
		return PMEM2_E_BUFFER_TOO_SMALL;
	}

	GUID guid;
	int ret = get_device_guid(src->handle, &guid);
	if (ret)
		return ret;

	_snwprintf(id, GUID_SIZE,
		L"%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",
		guid.Data1, guid.Data2, guid.Data3, guid.Data4[0],
		guid.Data4[1], guid.Data4[2], guid.Data4[3],
		guid.Data4[4], guid.Data4[5], guid.Data4[6],
		guid.Data4[7]);
	return 0;
}

int
pmem2_source_device_idU(const struct pmem2_source *src, char *id, size_t *len)
{
	if (id == NULL) {
		*len = GUID_SIZE * sizeof(*id);
		return 0;
	}
	if (*len < GUID_SIZE * sizeof(*id)) {
		ERR("id buffer is to small");
		return PMEM2_E_BUFFER_TOO_SMALL;
	}

	GUID guid;
	int ret = get_device_guid(src->handle, &guid);
	if (ret)
		return ret;

	if (util_snprintf(id, GUID_SIZE,
		"%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",
		guid.Data1, guid.Data2, guid.Data3, guid.Data4[0],
		guid.Data4[1], guid.Data4[2], guid.Data4[3],
		guid.Data4[4], guid.Data4[5], guid.Data4[6],
		guid.Data4[7]) < 0) {
		ERR("!snprintf");
		return PMEM2_E_ERRNO;
	}

	return 0;
}

int
pmem2_source_device_usc(const struct pmem2_source *src, uint64_t *usc)
{
	LOG(3, "cfg %p, usc %p", src, usc);
	*usc = 0;

	HANDLE vHandle;
	int err = get_volume_handle(src->handle, &vHandle);
	if (vHandle == INVALID_HANDLE_VALUE)
		return err;

	STORAGE_PROPERTY_QUERY prop;
	DWORD dwSize;
	prop.PropertyId = StorageDeviceUnsafeShutdownCount;
	prop.QueryType = PropertyExistsQuery;
	prop.AdditionalParameters[0] = 0;
	STORAGE_DEVICE_UNSAFE_SHUTDOWN_COUNT ret;

	BOOL bResult = DeviceIoControl(vHandle,
		IOCTL_STORAGE_QUERY_PROPERTY,
		&prop, sizeof(prop),
		&ret, sizeof(ret),
		(LPDWORD)&dwSize, (LPOVERLAPPED)NULL);

	if (!bResult) {
		ERR(
			"Getting unsafe shutdown count is not supported on this system");
		CloseHandle(vHandle);
		return PMEM2_E_NOSUPP;
	}
	prop.QueryType = PropertyStandardQuery;
	bResult = DeviceIoControl(vHandle,
		IOCTL_STORAGE_QUERY_PROPERTY,
		&prop, sizeof(prop),
		&ret, sizeof(ret),
		(LPDWORD)&dwSize, (LPOVERLAPPED)NULL);

	CloseHandle(vHandle);
	if (!bResult) {
		ERR("!!DeviceIoControl");
		return pmem2_lasterror_to_err();
	}

	*usc = ret.UnsafeShutdownCount;

	return 0;
}
