// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2020, Intel Corporation */

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
#include <getopt.h>
#include "util.h"

#define MAXPRINT 8192

static int Verbose;

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
print_file_size(HANDLE fh)
{
	LARGE_INTEGER filesize;
	FILE_COMPRESSION_INFO fci;

	BOOL ret = GetFileSizeEx(fh, &filesize);
	if (ret == FALSE) {
		out_err(L"GetFileSizeEx");
		return;
	}

	ret = GetFileInformationByHandleEx(fh, FileCompressionInfo,
			&fci, sizeof(fci));
	if (ret == FALSE) {
		out_err(L"GetFileInformationByHandleEx");
		return;
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
}

/*
 * set_file_size - set file length
 */
static int
set_file_size(HANDLE fh, size_t len)
{
	LARGE_INTEGER llen;
	llen.QuadPart = len;

	int ret = SetFilePointerEx(fh, llen, NULL, FILE_BEGIN);
	if (ret == FALSE) {
		out_err(L"SetFilePointerEx");
		return 1;
	}

	ret = SetEndOfFile(fh);
	if (ret == FALSE) {
		out_err(L"SetEndOfFile");
		return 1;
	}

	return 0;
}

/*
 * set_sparse_file -- creates sparse file of given size
 */
static int
set_sparse_file(HANDLE fh, int force)
{
	/* check if sparse files are supported */
	DWORD flags = 0;
	BOOL ret = GetVolumeInformationByHandleW(fh, NULL, 0, NULL, NULL,
			&flags, NULL, 0);
	if (ret == FALSE) {
		out_err(L"GetVolumeInformationByHandle");
		return 1;
	}

	if ((flags & FILE_SUPPORTS_SPARSE_FILES) == 0) {
		return force ? 1 : 0;
	}

	DWORD nbytes;
	ret = DeviceIoControl(fh, FSCTL_SET_SPARSE, NULL, 0, NULL,
			0, &nbytes, NULL);
	if (ret == FALSE) {
		out_err(L"DeviceIoControl(FSCTL_SET_SPARSE)");
		return 1;
	}

	return 0;
}

/*
 * set_compressed_file -- creates sparse file of given size
 */
static int
set_compressed_file(HANDLE fh, int force)
{
	/* check if sparse files are supported */
	DWORD flags = 0;
	BOOL ret = GetVolumeInformationByHandleW(fh, NULL, 0, NULL, NULL,
		&flags, NULL, 0);
	if (ret == FALSE) {
		out_err(L"GetVolumeInformationByHandle");
		return 1;
	}

	if ((flags & FILE_FILE_COMPRESSION) == 0) {
		return force ? 1 : 0;
	}

	DWORD nbytes;
	USHORT n = 1; /* magic undocumented value */
	ret = DeviceIoControl(fh, FSCTL_SET_COMPRESSION, &n, sizeof(n), NULL, 0,
		&nbytes, NULL);
	if (ret == FALSE) {
		out_err(L"DeviceIoControl(FSCTL_SET_COMPRESSION)");
		return 1;
	}
	return 0;
}

int
main(int argc, char *argv[])
{
	util_suppress_errmsg();
	wchar_t **wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
	for (int i = 0; i < argc; i++) {
		argv[i] = util_toUTF8(wargv[i]);
		if (argv[i] == NULL) {
			for (i--; i >= 0; i--)
				free(argv[i]);
			out_err(L"Error during arguments conversion\n");
			return 1;
		}
	}

	if (argc < 2) {
		fprintf(stderr, "Usage: %s filename len\n", argv[0]);
		exit(1);
	}

	int i = 1;
	int opt;
	DWORD create_mode = OPEN_ALWAYS;
	int compress = 0;
	int sparse = 0;
	int check = 0;
	int force = 0;
	long long len = 0;
	while ((opt = getopt(argc, argv, "vnscfl:")) != -1) {
		switch (opt) {
			case 'p':
				check = 1;
			case 'v':
				Verbose = 1;
				break;
			case 's':
				sparse = 1;
				break;
			case 'f':
				force = 1;
				break;
			case 'n':
				create_mode = CREATE_ALWAYS;
				break;
			case 'c':
				compress = 1;
				break;
			case 'l':
				len = atoll(optarg);
				if (len < 0) {
					out_err(L"Invalid file length: %lld.\n",
						len);
					exit(3);
				}
				break;
			default:
				out_err(L"Unknown option: \'%c\'.", argv[i][1]);
				exit(2);
		}
	}

	wchar_t *filename = util_toUTF16(argv[optind]);
	if (filename == NULL) {
		out_err(L"util_toUTF16");
		return 1;
	}

	/* create zero-length file */
	HANDLE fh = CreateFileW(filename, GENERIC_READ | GENERIC_WRITE,
				FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
				create_mode, FILE_ATTRIBUTE_NORMAL, NULL);
	util_free_UTF16(filename);
	if (fh == INVALID_HANDLE_VALUE) {
		out_err(L"CreateFile");
		return -1;
	}
	if (sparse && set_sparse_file(fh, force)) {
		return -1;
	}

	if (compress && set_compressed_file(fh, force)) {
		return -1;
	}

	if (len && set_file_size(fh, len)) {
		return -1;
	}

	if (Verbose)
		print_file_size(fh);

	CloseHandle(fh);

	for (int i = argc; i > 0; i--)
		free(argv[i - 1]);

	return 0;
}
