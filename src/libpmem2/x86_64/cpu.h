/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2016-2020, Intel Corporation */

#ifndef PMDK_CPU_H
#define PMDK_CPU_H 1

/*
 * cpu.h -- definitions for "cpu" module
 */

int is_cpu_genuine_intel(void);
int is_cpu_clflush_present(void);
int is_cpu_clflushopt_present(void);
int is_cpu_clwb_present(void);
int is_cpu_avx_present(void);
int is_cpu_avx512f_present(void);

#endif
