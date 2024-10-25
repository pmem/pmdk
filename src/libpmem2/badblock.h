/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2024, Intel Corporation */

/*
 * badblock.h -- badblock internal iterator function
 */

#ifndef PMEM2_BADBLOCK_H
#define PMEM2_BADBLOCK_H 1

#ifdef __cplusplus
extern "C" {
#endif

int pmem2_badblock_next_int(struct pmem2_badblock_context *bbctx,
				struct pmem2_badblock *bb, int warning);

#ifdef __cplusplus
}
#endif

#endif
