// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

/*
 * pmem_config.c -- pmem2_config unittests
 */
#include "fault_injection.h"
#include "unittest.h"
#include "ut_pmem2.h"
#include "config.h"
#include "out.h"
#include "source.h"

#define USC 42
#define UGUID  "0000002A-002A-002A-2A2A-2A2A2A2A2A2A"
#define WGUID L"0000002A-002A-002A-2A2A-2A2A2A2A2A2A"

#define GUID_SIZE 37 /* XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX */
#define WGUID_SIZE GUID_SIZE * sizeof(wchar_t)

#define MOCK_SUCCESS (1 << 31)
#define MOCK_NO = 0
#define MOCK_FAIL_ON(x) (x)
#define FUNC_MOCK_MODE_NAME(x) _##x##mode
#define FUNC_MOCK_MODE_DECL(x) static struct mock_mode FUNC_MOCK_MODE_NAME(x);

struct mock_mode {
	int fail_on;
};

#define MOCK_RESET(x) mock_set(&FUNC_MOCK_MODE_NAME(x), 0);
#define MOCK_COUTER_RESET(x) FUNC_MOCK_RCOUNTER_SET(x, 0);

#define MOCK_SET(x, f) mock_set(&FUNC_MOCK_MODE_NAME(x), f)

static inline void
mock_set(struct mock_mode *x, unsigned fail_on)
{
	x->fail_on = fail_on;
}

FUNC_MOCK_MODE_DECL(GetFinalPathNameByHandleW);
FUNC_MOCK_DLLIMPORT(GetFinalPathNameByHandleW, DWORD, HANDLE hFile,
	LPWSTR lpszFilePath, DWORD cchFilePath, DWORD dwFlags)
FUNC_MOCK_RUN_DEFAULT
{
	struct mock_mode *mode =
		&FUNC_MOCK_MODE_NAME(GetFinalPathNameByHandleW);

	if (mode->fail_on == RCOUNTER(GetFinalPathNameByHandleW)) {
		SetLastError(ERROR_FILE_NOT_FOUND); /* random error */
		return FALSE;
	}

	return _FUNC_REAL(GetFinalPathNameByHandleW)(hFile, lpszFilePath,
		cchFilePath, dwFlags);
}
FUNC_MOCK_END

FUNC_MOCK_MODE_DECL(CreateFileW);
FUNC_MOCK_DLLIMPORT(CreateFileW, HANDLE, LPCWSTR lpFileName,
	DWORD dwDesiredAccess, DWORD dwShareMode,
	LPSECURITY_ATTRIBUTES lpSecurityAttributes,
	DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes,
	HANDLE hTemplateFile)
FUNC_MOCK_RUN_DEFAULT
{
	struct mock_mode *mode = &FUNC_MOCK_MODE_NAME(CreateFileW);

	if (mode->fail_on == RCOUNTER(CreateFileW)) {
		SetLastError(ERROR_ACCESS_DENIED); /* random error */
		return INVALID_HANDLE_VALUE;
	}

	return _FUNC_REAL(CreateFileW)(lpFileName, dwDesiredAccess, dwShareMode,
		lpSecurityAttributes, dwCreationDisposition,
		dwFlagsAndAttributes, hTemplateFile);
}
FUNC_MOCK_END

FUNC_MOCK_MODE_DECL(DeviceIoControl);
FUNC_MOCK_DLLIMPORT(DeviceIoControl, BOOL, HANDLE hDevice,
	DWORD dwIoControlCode, LPVOID lpInBuffer,
	DWORD nInBufferSize, LPVOID lpOutBuffer,
	DWORD nOutBufferSize, LPDWORD lpBytesReturned,
	LPOVERLAPPED lpOverlapped)

FUNC_MOCK_RUN_DEFAULT
{
	struct mock_mode *mode = &FUNC_MOCK_MODE_NAME(DeviceIoControl);

	if (mode->fail_on == RCOUNTER(DeviceIoControl)) {
		SetLastError(ERROR_FILE_NOT_FOUND); /* random error */
		return FALSE;
	}
	if (dwIoControlCode == IOCTL_STORAGE_QUERY_PROPERTY) {
		STORAGE_PROPERTY_QUERY *prop = lpInBuffer;
		STORAGE_DEVICE_UNSAFE_SHUTDOWN_COUNT *ret = lpOutBuffer;
		if (prop->QueryType == PropertyStandardQuery)
			ret->UnsafeShutdownCount = USC;
	}
	if (dwIoControlCode == IOCTL_STORAGE_GET_DEVICE_NUMBER_EX) {
		STORAGE_DEVICE_NUMBER_EX *sdn = lpOutBuffer;
		GUID g;
		g.Data1 = 42;
		g.Data2 = 42;
		g.Data3 = 42;
		for (int i = 0; i < 8; i++)
			g.Data4[i] = 42;

		sdn->DeviceGuid = g;
	}
	return TRUE;
}
FUNC_MOCK_END

static void
mock_counter_reset()
{
	MOCK_COUTER_RESET(CreateFileW);
	MOCK_COUTER_RESET(GetFinalPathNameByHandleW);
	MOCK_COUTER_RESET(DeviceIoControl);
}

static void
mock_reset()
{
	mock_counter_reset();
	MOCK_RESET(CreateFileW);
	MOCK_RESET(GetFinalPathNameByHandleW);
	MOCK_RESET(DeviceIoControl);
}

static void
test_get_volume_handle_helper(struct pmem2_source *src, int err)
{
	size_t usc;
	size_t ulen = 128;
	char uid[128];
	wchar_t wuid[128];

	int ret = pmem2_source_device_usc(src, &usc);
	UT_ASSERTeq(ret, err);
	mock_counter_reset();

	ret = pmem2_source_device_idU(src, uid, &ulen);
	UT_ASSERTeq(ret, err);
	mock_counter_reset();

	ret = pmem2_source_device_idW(src, wuid, &ulen);
	UT_ASSERTeq(ret, err);
	mock_counter_reset();
}

static int
test_get_volume_handle(const struct test_case *tc, int argc,
		char *argv[])
{
	struct pmem2_source *src;
	const char *file = argv[0];
	HANDLE h = CreateFile(file, GENERIC_READ | GENERIC_WRITE, 0, NULL,
		OPEN_ALWAYS, 0, NULL);
	UT_ASSERTne(h, INVALID_HANDLE_VALUE);
	int ret = pmem2_source_from_handle(&src, h);
	UT_ASSERTeq(ret, 0);

	MOCK_SET(GetFinalPathNameByHandleW, 1);
	test_get_volume_handle_helper(src, -ENOENT);
	mock_reset();

	MOCK_SET(GetFinalPathNameByHandleW, 2);
	test_get_volume_handle_helper(src, -ENOENT);
	mock_reset();

	MOCK_SET(CreateFileW, 1);
	test_get_volume_handle_helper(src, -EACCES);
	mock_reset();

	return 1;
}

static int
test_pmem2_source_device_usc(const struct test_case *tc, int argc, char *argv[])
{
	struct pmem2_source *src;
	size_t usc = 72; /* random number */
	const char *file = argv[0];
	HANDLE h = CreateFile(file, GENERIC_READ | GENERIC_WRITE, 0, NULL,
		OPEN_ALWAYS, 0, NULL);

	UT_ASSERTne(h, INVALID_HANDLE_VALUE);
	int ret = pmem2_source_from_handle(&src, h);
	UT_ASSERTeq(ret, 0);

	MOCK_SET(DeviceIoControl, 1);
	ret = pmem2_source_device_usc(src, &usc);
	UT_ASSERTeq(ret, PMEM2_E_NOSUPP);
	UT_ASSERTeq(usc, 0);
	mock_reset();
	usc = 72; /* reset */

	MOCK_SET(DeviceIoControl, 2);
	ret = pmem2_source_device_usc(src, &usc);
	UT_ASSERTeq(ret, -ENOENT);
	UT_ASSERTeq(usc, 0);
	mock_reset();

	ret = pmem2_source_device_usc(src, &usc);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(usc, USC);
	return 1;
}

static int
test_pmem2_source_device_id(const struct test_case *tc, int argc, char *argv[])
{
	struct pmem2_source *src;
	size_t len;
	size_t size;
	size_t wsize;
	char id[GUID_SIZE];
	wchar_t wid[WGUID_SIZE];
	const char *file = argv[0];
	HANDLE h = CreateFile(file, GENERIC_READ | GENERIC_WRITE, 0, NULL,
		OPEN_ALWAYS, 0, NULL);
	UT_ASSERTne(h, INVALID_HANDLE_VALUE);
	int ret = pmem2_source_from_handle(&src, h);
	UT_ASSERTeq(ret, 0);

	pmem2_source_device_idU(src, NULL, &len);
	UT_ASSERTeq(len, GUID_SIZE);
	pmem2_source_device_idW(src, NULL, &len);
	UT_ASSERTeq(len, GUID_SIZE * sizeof(wchar_t));

	size = 0;
	wsize = 0;
	ret = pmem2_source_device_idU(src, id, &size);
	UT_ASSERTeq(ret, PMEM2_E_BUFFER_TOO_SMALL);
	ret = pmem2_source_device_idW(src, wid, &wsize);
	UT_ASSERTeq(ret, PMEM2_E_BUFFER_TOO_SMALL);

	size = GUID_SIZE;
	wsize = WGUID_SIZE;

	MOCK_SET(DeviceIoControl, 1);
	ret = pmem2_source_device_idU(src, id, &size);
	UT_ASSERTeq(ret, PMEM2_E_NOSUPP);
	mock_counter_reset();
	ret = pmem2_source_device_idW(src, wid, &wsize);
	UT_ASSERTeq(ret, PMEM2_E_NOSUPP);
	mock_reset();

	ret = pmem2_source_device_idU(src, id, &size);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(strcmp(id, UGUID), 0);
	ret = pmem2_source_device_idW(src, wid, &wsize);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(wcscmp(wid, WGUID), 0);
	return 1;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_get_volume_handle),
	TEST_CASE(test_pmem2_source_device_usc),
	TEST_CASE(test_pmem2_source_device_id),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char **argv)
{
	START(argc, argv, "pmem2_usc");

	util_init();
	out_init("pmem2_usc", "TEST_LOG_LEVEL", "TEST_LOG_FILE", 0, 0);
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	out_fini();

	DONE(NULL);
}
