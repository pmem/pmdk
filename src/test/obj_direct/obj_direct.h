/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2017-2020, Intel Corporation */

/*
 * obj_direct.h -- unit test for pmemobj_direct()
 */
#ifndef OBJ_DIRECT_H
#define OBJ_DIRECT_H 1

#include "libpmemobj.h"

void *obj_direct_inline(PMEMoid oid);
void *obj_direct_non_inline(PMEMoid oid);

#endif /* OBJ_DIRECT_H */
