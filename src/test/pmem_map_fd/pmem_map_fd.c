/*
 * Copyright 2017, Nippon Telegraph and Telephone Corporation
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
 * pmem_map_fd.c -- unit test for mapping persistent memory for raw access
 *
 * usage: pmem_map_fd file
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
	START(argc, argv, "pmem_map_fd");

	int fd;
	void *addr;
	size_t mlen, *mlenp;
	const char *path;
	int is_pmem, *is_pmemp, is_pmem_check;
	int use_mlen, use_is_pmem;

	if (argc < 4)
		UT_FATAL("usage: %s path use_mlen use_is_pmem ...",
			argv[0]);

	for (int i = 1; i + 2 < argc; i += 3) {
		path = argv[i];
		use_mlen = atoi(argv[i + 1]);
		use_is_pmem = atoi(argv[i + 2]);

		UT_OUT("%s %d %d", path, use_mlen, use_is_pmem);

		/* assume that path already exists */
		fd = OPEN(path, O_RDWR);

		mlen = SIZE_MAX;
		mlenp = (use_mlen) ? &mlen : NULL;

		is_pmemp = (use_is_pmem) ? &is_pmem : NULL;

		addr = pmem_map_fd(fd, mlenp, is_pmemp);
		if (addr == NULL) {
			UT_OUT("!pmem_map_fd");
			CLOSE(fd);
			continue;
		}

		os_stat_t stbuf;
		FSTAT(fd, &stbuf);
		UT_ASSERT(stbuf.st_size >= 0);

		if (use_mlen) {
			UT_OUT("mapped_len %zu", mlen);
			UT_ASSERTeq((size_t)stbuf.st_size, mlen);
		} else {
			mlen = (size_t)stbuf.st_size;
		}

		/* check is_pmem returned from pmem_map_fd */
		if (use_is_pmem) {
			is_pmem_check = pmem_is_pmem(addr, mlen);
			UT_ASSERTeq(is_pmem, is_pmem_check);
		}

		do_check(fd, addr, mlen); /* this should call pmem_unmap */
		CLOSE(fd);
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
