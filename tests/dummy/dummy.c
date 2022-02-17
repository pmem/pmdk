// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include <stdio.h>
#include <stdlib.h>

int main(void) {
		printf("what's happening with test output?\n");
		float *ptr = (float *)malloc(100 * sizeof(float));
		/* comment free to test memcheck */
		free(ptr);
		return 0;
}
