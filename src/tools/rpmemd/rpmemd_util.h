// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2018, Intel Corporation */

/*
 * rpmemd_util.h -- rpmemd utility functions declarations
 */

int rpmemd_pmem_persist(const void *addr, size_t len);
int rpmemd_flush_fatal(const void *addr, size_t len);
int rpmemd_apply_pm_policy(enum rpmem_persist_method *persist_method,
	int (**persist)(const void *addr, size_t len),
	void *(**memcpy_persist)(void *pmemdest, const void *src, size_t len),
	const int is_pmem);
