/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2019-2021, Intel Corporation */
/* Copyright 2019, IBM Corporation */

#ifndef PMDK_PAGE_SIZE_H
#define PMDK_PAGE_SIZE_H

#if defined(__x86_64) || defined(_M_X64) || defined(__aarch64__) || \
	defined(__riscv)

#define PMEM_PAGESIZE 4096

#elif defined(__PPC64__)

#define PMEM_PAGESIZE 65536

#else

#error unable to recognize ISA at compile time

#endif

#endif  /* PMDK_PAGE_SIZE_H */
