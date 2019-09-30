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
 * sparsefile.c -- a simple utility to create sparse files on Windows
 *
 * usage: sparsefile [options] filename len
 * where options can be:
 *    -v - verbose output
 *    -s - do not create file if sparse files are not supported
 *    -f - overwrite file if already exists
 */

#include <windows.h>
#include <stdio.h>
#include "util.h"

#define MAXPRINT 8192

static int Opt_verbose;
static int Opt_sparse;
static int Opt_force;

/*
 * out_err_vargs -- print error message
 */
static void
out_err_vargs(const wchar_t *fmt, va_list ap)
{
	wchar_t errmsg[MAXPRINT];
	DWORD lasterr = GetLastError();

	vfwprintf(stderr, fmt, ap);
	if (lasterr) {
		size_t size = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM,
				NULL, lasterr,
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				errmsg, MAXPRINT, NULL);
		fwprintf(stderr, L": %s", errmsg);
	} else {
		fwprintf(stderr, L"\n");
	}

	SetLastError(0);
}

/*
 * out_err -- print error message
 */
static void
out_err(const wchar_t *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	out_err_vargs(fmt, ap);
	va_end(ap);
}

/*
 * print_file_size -- prints file size and its size on disk
 */
static void
print_file_size(const wchar_t *filename)
{
	LARGE_INTEGER filesize;
	FILE_COMPRESSION_INFO fci;

	HANDLE fh = CreateFileW(filename, GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (fh == INVALID_HANDLE_VALUE) {
		out_err(L"CreateFile");
		return;
	}

	BOOL ret = GetFileSizeEx(fh, &filesize);
	if (ret == FALSE) {
		out_err(L"GetFileSizeEx");
		goto err;
	}

	ret = GetFileInformationByHandleEx(fh, FileCompressionInfo,
			&fci, sizeof(fci));
	if (ret == FALSE) {
		out_err(L"GetFileInformationByHandleEx");
		goto err;
	}

	if (filesize.QuadPart < 65536)
		fwprintf(stderr, L"\ntotal size: %lluB",
				filesize.QuadPart);
	else
		fwprintf(stderr, L"\ntotal size: %lluKB",
				filesize.QuadPart / 1024);

	if (fci.CompressedFileSize.QuadPart < 65536)
		fwprintf(stderr, L", actual size on disk: %lluKB\n",
				fci.CompressedFileSize.QuadPart);
	else
		fwprintf(stderr, L", actual size on disk: %lluKB\n",
				fci.CompressedFileSize.QuadPart / 1024);

err:
	CloseHandle(fh);
}

/*
 * create_sparse_file -- creates sparse file of given size
 */
static int
create_sparse_file(const wchar_t *filename, size_t len)
{
	/* create zero-length file */
	DWORD create = Opt_force ? CREATE_ALWAYS : CREATE_NEW;
	HANDLE fh = CreateFileW(filename, GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
			create, FILE_ATTRIBUTE_NORMAL, NULL);
	if (fh == INVALID_HANDLE_VALUE) {
		out_err(L"CreateFile");
		return -1;
	}
	SetLastError(0);

	/* check if sparse files are supported */
	DWORD flags = 0;
	BOOL ret = GetVolumeInformationByHandleW(fh, NULL, 0, NULL, NULL,
			&flags, NULL, 0);
	if (ret == FALSE) {
		if (Opt_verbose || Opt_sparse)
			out_err(L"GetVolumeInformationByHandle");
	} else if ((flags & FILE_SUPPORTS_SPARSE_FILES) == 0) {
		if (Opt_verbose || Opt_sparse)
			out_err(L"Volume does not support sparse files.");
		if (Opt_sparse)
			goto err;
	}

	/* mark file as sparse */
	if (flags & FILE_SUPPORTS_SPARSE_FILES) {
		DWORD nbytes;
		ret = DeviceIoControl(fh, FSCTL_SET_SPARSE, NULL, 0, NULL,
				0, &nbytes, NULL);
		if (ret == FALSE) {
			if (Opt_verbose || Opt_sparse)
				out_err(L"DeviceIoControl");
			if (Opt_sparse)
				goto err;
		}
	}

	/* set file length */
	LARGE_INTEGER llen;
	llen.QuadPart = len;

	ret = SetFilePointerEx(fh, llen, NULL, FILE_BEGIN);
	if (ret == FALSE) {
		out_err(L"SetFilePointerEx");
		goto err;
	}

	ret = SetEndOfFile(fh);
	if (ret == FALSE) {
		out_err(L"SetEndOfFile");
		goto err;
	}

	CloseHandle(fh);
	return 0;

err:
	CloseHandle(fh);
	DeleteFileW(filename);
	return -1;
}

int
wmain(int argc, const wchar_t *argv[])
{
	util_suppress_errmsg();
	if (argc < 2) {
		fwprintf(stderr, L"Usage: %s filename len\n", argv[0]);
		exit(1);
	}

	int i = 1;
	while (i < argc && argv[i][0] == '-') {
		switch (argv[i][1]) {
			case 'v':
				Opt_verbose = 1;
				break;
			case 's':
				Opt_sparse = 1;
				break;
			case 'f':
				Opt_force = 1;
				break;
			default:
				out_err(L"Unknown option: \'%c\'.", argv[i][1]);
				exit(2);
		}
		++i;
	}

	const wchar_t *filename = argv[i];
	long long len = _wtoll(argv[i + 1]);

	if (len < 0) {
		out_err(L"Invalid file length: %lld.\n", len);
		exit(3);
	}

	if (create_sparse_file(filename, len) < 0) {
		out_err(L"File creation failed.");
		exit(4);
	}

	if (Opt_verbose)
		print_file_size(filename);

	return 0;
}
