/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2024, Intel Corporation */

/*
 * badblocks.h -- badblock internal iterator function
 */

#ifndef PMEM2_BADBLOCKS_H
#define PMEM2_BADBLOCKS_H 1

#ifdef __cplusplus
extern "C" {
#endif

int pmem2_badblock_next_internal(struct pmem2_badblock_context *bbctx,
				struct pmem2_badblock *bb);

#ifdef __cplusplus
}
#endif

#endif /* PMEM2_BADBLOCKS_H */
