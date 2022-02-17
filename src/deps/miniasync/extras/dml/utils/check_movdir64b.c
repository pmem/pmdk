// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "cpu.h"

int
main()
{
	if (!is_cpu_movdir64b_present())
		return -1;

	return 0;
}
