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
 * pmem_map_file_win.c -- unit test for mapping persistent memory for raw access
 *
 * usage: pmem_map_file_win file
 */

#define _GNU_SOURCE
#include "unittest.h"
#include <stdlib.h>

#define CHECK_BYTES 4096	/* bytes to compare before/after map call */

ut_jmp_buf_t Jmp;

/*
 * signal_handler -- called on SIGSEGV
 */
static void
signal_handler(int sig)
{
	ut_siglongjmp(Jmp);
}

#define PMEM_FILE_ALL_FLAGS\
	(PMEM_FILE_CREATE|PMEM_FILE_EXCL|PMEM_FILE_SPARSE|PMEM_FILE_TMPFILE)

static int device_dax = 0;

/*
 * parse_flags -- parse 'flags' string
 */
static int
parse_flags(const wchar_t *flags_str)
{
	int ret = 0;
	while (*flags_str != L'\0') {
		switch (*flags_str) {
		case L'0':
		case L'-':
			/* no flags */
			break;
		case L'T':
			ret |= PMEM_FILE_TMPFILE;
			break;
		case L'S':
			ret |= PMEM_FILE_SPARSE;
			break;
		case L'C':
			ret |= PMEM_FILE_CREATE;
			break;
		case L'E':
			ret |= PMEM_FILE_EXCL;
			break;
		case L'X':
			/* not supported flag */
			ret |= (PMEM_FILE_ALL_FLAGS + 1);
			break;
		case L'D':
			device_dax = 1;
			break;
		default:
			UT_FATAL("unknown flags: %c", *flags_str);
		}
		flags_str++;
	};
	return ret;
}

/*
 * do_check -- check the mapping
 */
static void
do_check(int fd, void *addr, size_t mlen)
{
	/* arrange to catch SEGV */
	struct sigaction v;
	sigemptyset(&v.sa_mask);
	v.sa_flags = 0;
	v.sa_handler = signal_handler;
	SIGACTION(SIGSEGV, &v, NULL);

	char pat[CHECK_BYTES];
	char buf[CHECK_BYTES];

	/* write some pattern to the file */
	memset(pat, 0x5A, CHECK_BYTES);
	WRITE(fd, pat, CHECK_BYTES);

	if (memcmp(pat, addr, CHECK_BYTES))
		UT_OUT("first %d bytes do not match", CHECK_BYTES);

	/* fill up mapped region with new pattern */
	memset(pat, 0xA5, CHECK_BYTES);
	memcpy(addr, pat, CHECK_BYTES);

	UT_ASSERTeq(pmem_msync(addr, CHECK_BYTES), 0);

	UT_ASSERTeq(pmem_unmap(addr, mlen), 0);

	if (!ut_sigsetjmp(Jmp)) {
		/* same memcpy from above should now fail */
		memcpy(addr, pat, CHECK_BYTES);
	} else {
		UT_OUT("unmap successful");
	}

	LSEEK(fd, (os_off_t)0, SEEK_SET);
	if (READ(fd, buf, CHECK_BYTES) == CHECK_BYTES) {
		if (memcmp(pat, buf, CHECK_BYTES))
			UT_OUT("first %d bytes do not match", CHECK_BYTES);
	}
}

int
wmain(int argc, wchar_t *argv[])
{
	STARTW(argc, argv, "pmem_map_file_win");

	int fd;
	void *addr;
	size_t mlen;
	size_t *mlenp;
	const wchar_t *path;
	unsigned long long len;
	int flags;
	int mode;
	int is_pmem;
	int *is_pmemp;
	int use_mlen;
	int use_is_pmem;

	if (argc < 7)
		UT_FATAL("usage: %s path len flags mode use_mlen "
				"use_is_pmem ...", ut_toUTF8(argv[0]));

	for (int i = 1; i + 5 < argc; i += 6) {
		path = argv[i];
		len = wcstoull(argv[i + 1], NULL, 0);
		flags = parse_flags(argv[i + 2]);
		mode = wcstol(argv[i + 3], NULL, 8);
		use_mlen = _wtoi(argv[i + 4]);
		use_is_pmem = _wtoi(argv[i + 5]);

		mlen = SIZE_MAX;
		if (use_mlen)
			mlenp = &mlen;
		else
			mlenp = NULL;

		if (use_is_pmem)
			is_pmemp = &is_pmem;
		else
			is_pmemp = NULL;

		char *upath = ut_toUTF8(path);
		char *uflags = ut_toUTF8(argv[i + 2]);
		UT_OUT("%s %lld %s %o %d %d",
			upath, len, uflags, mode, use_mlen, use_is_pmem);
		free(uflags);
		free(upath);

		addr = pmem_map_fileW(path, len, flags, mode, mlenp, is_pmemp);
		if (addr == NULL) {
			UT_OUT("!pmem_map_file");
			continue;
		}

		if (use_mlen) {
			UT_ASSERTne(mlen, SIZE_MAX);
			UT_OUT("mapped_len %zu", mlen);
		} else {
			mlen = len;
		}

		if (addr) {
			if ((flags & PMEM_FILE_TMPFILE) == 0 && !device_dax) {
				fd = WOPEN(argv[i], O_RDWR);

				if (!use_mlen) {
					os_stat_t stbuf;
					FSTAT(fd, &stbuf);
					mlen = stbuf.st_size;
				}

				if (fd != -1) {
					do_check(fd, addr, mlen);
					(void) CLOSE(fd);
				} else {
					UT_OUT("!cannot open file: %s",
							argv[i]);
				}
			} else {
				UT_ASSERTeq(pmem_unmap(addr, mlen), 0);
			}
		}
	}

	DONEW(NULL);
}

/*
 * Since libpmem is linked statically, we need to invoke its ctor/dtor.
 */
MSVC_CONSTR(libpmem_init)
MSVC_DESTR(libpmem_fini)
