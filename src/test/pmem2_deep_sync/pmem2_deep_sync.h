// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * pmem2_deep_sync.h -- header file for for pmem2_deep_sync
 */
#ifndef PMEM_DEEP_SYNC_H
#define PMEM_DEEP_SYNC_H 1

extern int n_msynces;
extern int n_persists;
extern int is_devdax;

void pmem2_persist_mock(const void *addr, size_t len);

#endif
