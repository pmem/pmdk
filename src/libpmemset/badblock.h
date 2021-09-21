/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2021, Intel Corporation */

/*
 * badblock.h -- internal definitions for badblock module
 */
#ifndef PMEMSET_BADBLOCK_H
#define PMEMSET_BADBLOCK_H

struct pmemset;
struct pmemset_source;

int pmemset_badblock_detect_check_if_cleared(struct pmemset *set,
		struct pmemset_source *src);

#endif /* PMEMSET_BADBLOCK_H */
