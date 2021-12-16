/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2022, Intel Corporation */

/*
 * mover.h -- implementation of default datamover
 */
#ifndef PMEM2_MOVER_H
#define PMEM2_MOVER_H

#include "libpmem2.h"
#include "libminiasync/vdm.h"

int mover_new(struct pmem2_map *map, struct vdm **vdm);
void mover_delete(struct vdm *vdm);

#endif /* PMEM2_MOVER_H */
