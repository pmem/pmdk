// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018, Intel Corporation */

/*
 * ctl_debug.h -- definitions for the debug CTL namespace
 */
#ifndef LIBPMEMOBJ_CTL_DEBUG_H
#define LIBPMEMOBJ_CTL_DEBUG_H 1

#include "libpmemobj.h"

#ifdef __cplusplus
extern "C" {
#endif

void debug_ctl_register(PMEMobjpool *pop);

#ifdef __cplusplus
}
#endif

#endif /* LIBPMEMOBJ_CTL_DEBUG_H */
