/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2022, Intel Corporation */

#ifndef MINIASYNC_CPU_H
#define MINIASYNC_CPU_H 1

/*
 * cpu.h -- definitions for "cpu" module
 */

#if defined(__x86_64__) || defined(__amd64__) || defined(_M_X64) || \
	defined(_M_AMD64)

int is_cpu_movdir64b_present(void);

#endif

#endif
