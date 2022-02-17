/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2022, Intel Corporation */

#ifndef DATA_MOVER_SYNC_H
#define DATA_MOVER_SYNC_H

#include "vdm.h"

#ifdef __cplusplus
extern "C" {
#endif

struct data_mover_sync;

struct data_mover_sync *data_mover_sync_new(void);

struct vdm *data_mover_sync_get_vdm(struct data_mover_sync *dms);

void data_mover_sync_delete(struct data_mover_sync *dms);

#ifdef __cplusplus
}
#endif
#endif /* DATA_MOVER_SYNC_H */
