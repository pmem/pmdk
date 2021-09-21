/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2021, Intel Corporation */

/*
 * badblock.h -- internal definitions for badblock module
 */
#ifndef PMEMSET_BADBLOCK_H
#define PMEMSET_BADBLOCK_H

struct pmemset;
struct pmemset_badblock;
struct pmemset_source;

/*
 * typedef forearch callback function invoked on each iteration of badblock
 * contained in the source
 */
typedef int pmemset_bb_foreach_cb(struct pmemset_badblock *bb,
		struct pmemset *set, struct pmemset_source *src);

int pmemset_badblock_foreach(struct pmemset *set, struct pmemset_source *src,
		pmemset_bb_foreach_cb cb, size_t *count);

int pmemset_badblock_fire_badblock_event(struct pmemset_badblock *bb,
		struct pmemset *set, struct pmemset_source *src);

#endif /* PMEMSET_BADBLOCK_H */
