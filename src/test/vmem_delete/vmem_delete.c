/*
 * Copyright 2014-2016, Intel Corporation
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
 * vmem_delete.c -- unit test for vmem_delete
 *
 * usage: vmem_delete <operation>
 *
 * operations are: 'h', 'f', 'm', 'c', 'r', 'a', 's', 'd'
 *
 */

#include "unittest.h"

sigjmp_buf Jmp;

/*
 * signal_handler -- called on SIGSEGV
 */
static void
signal_handler(int sig)
{
	UT_OUT("\tsignal: %s", strsignal(sig));

	siglongjmp(Jmp, 1);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "vmem_delete");

	VMEM *vmp;
	void *ptr;

	if (argc < 2)
		UT_FATAL("usage: %s op:h|f|m|c|r|a|s|d", argv[0]);

	/* allocate memory for function vmem_create_in_region() */
	void *mem_pool = MMAP_ANON_ALIGNED(VMEM_MIN_POOL, 4 << 20);

	vmp = vmem_create_in_region(mem_pool, VMEM_MIN_POOL);
	if (vmp == NULL)
		UT_FATAL("!vmem_create_in_region");

	ptr = vmem_malloc(vmp, sizeof(long long int));
	if (ptr == NULL)
		UT_ERR("!vmem_malloc");
	vmem_delete(vmp);

	/* arrange to catch SEGV */
	struct sigaction v;
	sigemptyset(&v.sa_mask);
	v.sa_flags = 0;
	v.sa_handler = signal_handler;
	SIGACTION(SIGSEGV, &v, NULL);
	SIGACTION(SIGABRT, &v, NULL);
	SIGACTION(SIGILL, &v, NULL);

	/* go through all arguments one by one */
	for (int arg = 1; arg < argc; arg++) {
		/* Scan the character of each argument. */
		if (strchr("hfmcrasd", argv[arg][0]) == NULL ||
				argv[arg][1] != '\0')
			UT_FATAL("op must be one of: h, f, m, c, r, a, s, d");

		switch (argv[arg][0]) {
		case 'h':
			UT_OUT("Testing vmem_check...");
			if (!sigsetjmp(Jmp, 1)) {
				UT_OUT("\tvmem_check returned %i",
							vmem_check(vmp));
			}
			break;

		case 'f':
			UT_OUT("Testing vmem_free...");
			if (!sigsetjmp(Jmp, 1)) {
				vmem_free(vmp, ptr);
				UT_OUT("\tvmem_free succeeded");
			}
			break;

		case 'm':
			UT_OUT("Testing vmem_malloc...");
			if (!sigsetjmp(Jmp, 1)) {
				ptr = vmem_malloc(vmp, sizeof(long long int));
				if (ptr != NULL)
					UT_OUT("\tvmem_malloc succeeded");
				else
					UT_OUT("\tvmem_malloc returned NULL");
			}
			break;

		case 'c':
			UT_OUT("Testing vmem_calloc...");
			if (!sigsetjmp(Jmp, 1)) {
				ptr = vmem_calloc(vmp, 10, sizeof(int));
				if (ptr != NULL)
					UT_OUT("\tvmem_calloc succeeded");
				else
					UT_OUT("\tvmem_calloc returned NULL");
			}
			break;

		case 'r':
			UT_OUT("Testing vmem_realloc...");
			if (!sigsetjmp(Jmp, 1)) {
				ptr = vmem_realloc(vmp, ptr, 128);
				if (ptr != NULL)
					UT_OUT("\tvmem_realloc succeeded");
				else
					UT_OUT("\tvmem_realloc returned NULL");
			}
			break;

		case 'a':
			UT_OUT("Testing vmem_aligned_alloc...");
			if (!sigsetjmp(Jmp, 1)) {
				ptr = vmem_aligned_alloc(vmp, 128, 128);
				if (ptr != NULL)
					UT_OUT("\tvmem_aligned_alloc "
						"succeeded");
				else
					UT_OUT("\tvmem_aligned_alloc"
							" returned NULL");
			}
			break;

		case 's':
			UT_OUT("Testing vmem_strdup...");
			if (!sigsetjmp(Jmp, 1)) {
				ptr = vmem_strdup(vmp, "Test string");
				if (ptr != NULL)
					UT_OUT("\tvmem_strdup succeeded");
				else
					UT_OUT("\tvmem_strdup returned NULL");
			}
			break;

		case 'd':
			UT_OUT("Testing vmem_delete...");
			if (!sigsetjmp(Jmp, 1)) {
				vmem_delete(vmp);
				if (errno != 0)
					UT_OUT("\tvmem_delete failed: %s",
						vmem_errormsg());
				else
					UT_OUT("\tvmem_delete succeeded");
			}
			break;
		}
	}

	DONE(NULL);
}
