/*
 * Copyright 2019, Intel Corporation
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
 * ras_window.c -- pmem2 ras function
 */

#include "alloc.h"
#include "config.h"
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
	int len = GetFinalPathNameByHandleW(handle, tmp, 10, VOLUME_NAME_GUID);

	if (len <= 0) {
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
		CloseHandle(vHandle);
		return PMEM2_E_NOSUPP;
	}
	*guid = sdn.DeviceGuid;

	CloseHandle(vHandle);
	return 0;
}

int
pmem2_get_device_idW(const struct pmem2_config *cfg, wchar_t *id, size_t *len)
{
	if (id == NULL) {
		*len = GUID_SIZE * sizeof(*id);
		return 0;
	}

	if (*len < GUID_SIZE * sizeof(*id)) {
		ERR("id buffer is to small");
		return PMEM2_E_INVALID_ARG;
	}

	GUID guid;
	int ret = get_device_guid(cfg->handle, &guid);
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
pmem2_get_device_idU(const struct pmem2_config *cfg, char *id, size_t *len)
{
	if (id == NULL) {
		*len = GUID_SIZE * sizeof(*id);
		return 0;
	}
	if (*len < GUID_SIZE * sizeof(*id)) {
		ERR("id buffer is to small");
		return PMEM2_E_INVALID_ARG;
	}

	GUID guid;
	int ret = get_device_guid(cfg->handle, &guid);
	if (ret)
		return ret;

	snprintf(id, GUID_SIZE,
		"%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",
		guid.Data1, guid.Data2, guid.Data3, guid.Data4[0],
		guid.Data4[1], guid.Data4[2], guid.Data4[3],
		guid.Data4[4], guid.Data4[5], guid.Data4[6],
		guid.Data4[7]);
	return 0;
}

int
pmem2_get_device_usc(const struct pmem2_config *cfg, uint64_t *usc)
{
	LOG(3, "cfg %p, usc %p", cfg, usc);
	*usc = 0;

	HANDLE vHandle;
	int err = get_volume_handle(cfg->handle, &vHandle);
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
	if (!bResult)
		return -1;

	*usc = ret.UnsafeShutdownCount;

	return 0;
}

int
pmem2_badblock_iterator_new(const struct pmem2_config *cfg,
		struct pmem2_badblock_iterator **pbb)
{
	return PMEM2_E_NOSUPP;
}

int
pmem2_badblock_next(struct pmem2_badblock_iterator *pbb,
		struct pmem2_badblock *bb)
{
	return PMEM2_E_NOSUPP;
}

void pmem2_badblock_iterator_delete(
		struct pmem2_badblock_iterator **pbb)
{
}

int
pmem2_badblock_clear(const struct pmem2_config *cfg,
		const struct pmem2_badblock *bb)
{
	return PMEM2_E_NOSUPP;
}
