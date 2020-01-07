// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2019, Intel Corporation */

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
	return 0;
}
