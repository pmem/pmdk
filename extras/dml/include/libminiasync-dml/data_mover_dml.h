/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2021-2022, Intel Corporation */

#ifndef VDM_DML_H
#define VDM_DML_H 1

#include <dml/dml.h>
#include <libminiasync.h>

#ifdef __cplusplus
extern "C" {
#endif

struct data_mover_dml;

struct data_mover_dml *data_mover_dml_new(void);
struct vdm *data_mover_dml_get_vdm(struct data_mover_dml *dmd);
void data_mover_dml_delete(struct data_mover_dml *dmd);

#ifdef __cplusplus
}
#endif
#endif /* VDM_DML_H */
