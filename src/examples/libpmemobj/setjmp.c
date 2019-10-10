/*
 * Copyright 2016-2019, Intel Corporation
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
 * setjmp.c -- example illustrating an issue with indeterminate value
 * of non-volatile automatic variables after transaction abort.
 * See libpmemobj(7) for details.
 *
 * NOTE: To observe the problem (likely segfault on a second call to free()),
 * the example program should be compiled with optimizations enabled (-O2).
 */

#include <stdlib.h>
#include <stdio.h>
#include <libpmemobj.h>

/* name of our layout in the pool */
#define LAYOUT_NAME "setjmp_example"

int
main(int argc, const char *argv[])
{
	const char path[] = "/pmem-fs/myfile";
	PMEMobjpool *pop;

	/* create the pmemobj pool */
	pop = pmemobj_create(path, LAYOUT_NAME, PMEMOBJ_MIN_POOL, 0666);
	if (pop == NULL) {
		perror(path);
		exit(1);
	}

	/* initialize pointer variables with invalid addresses */
	int *bad_example_1 = (int *)0xBAADF00D;
	int *bad_example_2 = (int *)0xBAADF00D;
	int *bad_example_3 = (int *)0xBAADF00D;
	int *volatile good_example = (int *)0xBAADF00D;

	TX_BEGIN(pop) {
		bad_example_1 = malloc(sizeof(int));
		bad_example_2 = malloc(sizeof(int));
		bad_example_3 = malloc(sizeof(int));
		good_example = malloc(sizeof(int));

		/* manual or library abort called here */
		pmemobj_tx_abort(EINVAL);
	} TX_ONCOMMIT {
		/*
		 * This section is longjmp-safe
		 */
	} TX_ONABORT {
		/*
		 * This section is not longjmp-safe
		 */
		free(good_example); /* OK */
		free(bad_example_1); /* undefined behavior */
	} TX_FINALLY {
		/*
		 * This section is not longjmp-safe on transaction abort only
		 */
		free(bad_example_2); /* undefined behavior */
	} TX_END

	free(bad_example_3); /* undefined behavior */

	pmemobj_close(pop);
}
