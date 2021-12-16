/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2019-2020, Intel Corporation */

/*
 * mover.h -- implementation of default datamover
 */
#ifndef PMEM2_MOVER_H
#define PMEM2_MOVER_H

#include "libpmem2.h"
#include "libminiasync/vdm.h"

struct vdm *mover_new(struct pmem2_map *map);
void mover_delete(struct vdm *vdm);

#endif /* PMEM2_MOVER_H */
