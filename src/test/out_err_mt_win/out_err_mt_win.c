/*
 * Copyright 2016-2018, Intel Corporation
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
 * out_err_mt_win.c -- unit test for error messages
 */

#include <sys/types.h>
#include <stdarg.h>
#include <errno.h>
#include "unittest.h"
#include "valgrind_internal.h"
#include "util.h"

#define NUM_THREADS 16

static void
print_errors(const wchar_t *msg)
{
	UT_OUT("%S", msg);
	UT_OUT("VMEM: %S", vmem_errormsgW());
}

static void
check_errors(int ver)
{
	int ret;
	int err_need;
	int err_found;

	ret = swscanf(vmem_errormsgW(),
		L"libvmem major version mismatch (need %d, found %d)",
		&err_need, &err_found);
	UT_ASSERTeq(ret, 2);
	UT_ASSERTeq(err_need, ver);
	UT_ASSERTeq(err_found, VMEM_MAJOR_VERSION);
}

static void *
do_test(void *arg)
{
	int ver = *(int *)arg;

	vmem_check_version(ver, 0);
	check_errors(ver);

	return NULL;
}

static void
run_mt_test(void *(*worker)(void *))
{
	os_thread_t thread[NUM_THREADS];
	int ver[NUM_THREADS];

	for (int i = 0; i < NUM_THREADS; ++i) {
		ver[i] = 10000 + i;
		PTHREAD_CREATE(&thread[i], NULL, worker, &ver[i]);
	}
	for (int i = 0; i < NUM_THREADS; ++i) {
		PTHREAD_JOIN(&thread[i], NULL);
	}
}

int
wmain(int argc, wchar_t *argv[])
{
	STARTW(argc, argv, "out_err_mt_win");

	if (argc != 6)
		UT_FATAL("usage: %S file1 file2 file3 file4 dir",
				argv[0]);

	print_errors(L"start");

	VMEM *vmp = vmem_createW(argv[5], VMEM_MIN_POOL);

	util_init();

	vmem_check_version(10005, 0);
	print_errors(L"version check");

	VMEM *vmp2 = vmem_create_in_region(NULL, 1);
	UT_ASSERTeq(vmp2, NULL);
	print_errors(L"vmem_create_in_region");

	run_mt_test(do_test);

	vmem_delete(vmp);

	DONEW(NULL);
}
