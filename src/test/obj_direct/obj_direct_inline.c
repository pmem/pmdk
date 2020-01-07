// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017, Intel Corporation */

/*
 * obj_direct_inline.c -- unit test for direct
 */
#include "unittest.h"
#include "obj_direct.h"

void *
obj_direct_inline(PMEMoid oid)
{
	UT_OUT("pmemobj_direct inlined");
	return pmemobj_direct(oid);
}
