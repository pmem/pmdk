/*
 * Copyright 2014-2017, Intel Corporation
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
 * pmem_map_file.c -- unit test for mapping persistent memory for raw access
 *
 * usage: pmem_map_file file
 */

#define _GNU_SOURCE
#include "unittest.h"
#include <stdlib.h>

#define CHECK_BYTES 4096	/* bytes to compare before/after map call */

static ut_jmp_buf_t Jmp;

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

static int is_dev_dax = 0;

/*
 * parse_flags -- parse 'flags' string
 */
static int
parse_flags(const char *flags_str)
{
	int ret = 0;
	while (*flags_str != '\0') {
		switch (*flags_str) {
		case '0':
		case '-':
			/* no flags */
			break;
		case 'T':
			ret |= PMEM_FILE_TMPFILE;
			break;
		case 'S':
			ret |= PMEM_FILE_SPARSE;
			break;
		case 'C':
			ret |= PMEM_FILE_CREATE;
			break;
		case 'E':
			ret |= PMEM_FILE_EXCL;
			break;
		case 'X':
			/* not supported flag */
			ret |= (PMEM_FILE_ALL_FLAGS + 1);
			break;
		case 'D':
			is_dev_dax = 1;
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
main(int argc, char *argv[])
{
	START(argc, argv, "pmem_map_file");

	int fd;
	void *addr;
	size_t mlen;
	size_t *mlenp;
	const char *path;
	unsigned long long len;
	int flags;
	int mode;
	int is_pmem;
	int *is_pmemp;
	int use_mlen;
	int use_is_pmem;

	if (argc < 7)
		UT_FATAL("usage: %s path len flags mode use_mlen "
				"use_is_pmem ...", argv[0]);

	for (int i = 1; i + 5 < argc; i += 6) {
		path = argv[i];
		len = strtoull(argv[i + 1], NULL, 0);
		flags = parse_flags(argv[i + 2]);
		mode = strtol(argv[i + 3], NULL, 8);
		use_mlen = atoi(argv[i + 4]);
		use_is_pmem = atoi(argv[i + 5]);

		mlen = SIZE_MAX;
		if (use_mlen)
			mlenp = &mlen;
		else
			mlenp = NULL;

		if (use_is_pmem)
			is_pmemp = &is_pmem;
		else
			is_pmemp = NULL;

		UT_OUT("%s %lld %s %o %d %d",
			path, len, argv[i + 2], mode, use_mlen, use_is_pmem);

		addr = pmem_map_file(path, len, flags, mode, mlenp, is_pmemp);
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
			/* is_pmem must be true for device DAX */
			int is_pmem_check = pmem_is_pmem(addr, mlen);
			UT_ASSERT(!is_dev_dax || is_pmem_check);

			/* check is_pmem returned from pmem_map_file */
			if (use_is_pmem)
				UT_ASSERTeq(is_pmem, is_pmem_check);

			if ((flags & PMEM_FILE_TMPFILE) == 0 && !is_dev_dax) {
				fd = OPEN(argv[i], O_RDWR);

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

	DONE(NULL);
}


#ifdef _MSC_VER
/*
 * Since libpmem is linked statically, we need to invoke its ctor/dtor.
 */
MSVC_CONSTR(libpmem_init)
MSVC_DESTR(libpmem_fini)
#endif
