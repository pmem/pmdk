// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019, Intel Corporation */

/*
 * pmem2_compat.c -- compatibility test for libpmem vs libpmem2
 */

#include "unittest.h"

int
main(int argc, char *argv[])
{
	UT_COMPILE_ERROR_ON(PMEM_F_MEM_NODRAIN != PMEM2_F_MEM_NODRAIN);
	UT_COMPILE_ERROR_ON(PMEM_F_MEM_NONTEMPORAL != PMEM2_F_MEM_NONTEMPORAL);
	UT_COMPILE_ERROR_ON(PMEM_F_MEM_TEMPORAL != PMEM2_F_MEM_TEMPORAL);
	UT_COMPILE_ERROR_ON(PMEM_F_MEM_WC != PMEM2_F_MEM_WC);
	UT_COMPILE_ERROR_ON(PMEM_F_MEM_WB != PMEM2_F_MEM_WB);
	UT_COMPILE_ERROR_ON(PMEM_F_MEM_NOFLUSH != PMEM2_F_MEM_NOFLUSH);

	return 0;
}
