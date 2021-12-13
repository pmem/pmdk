// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
		printf("what's happening with test output?\n");
		float *ptr = (float *)malloc(100 * sizeof(float));
		/* comment free to test memcheck */
		free(ptr);
		return 0;
}
