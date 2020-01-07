// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2018, Intel Corporation */

/*
 * rpmemd_fip.h -- rpmemd libfabric provider module header file
 */

#include <stddef.h>

struct rpmemd_fip;

struct rpmemd_fip_attr {
	void *addr;
	size_t size;
	unsigned nlanes;
	size_t nthreads;
	size_t buff_size;
	enum rpmem_provider provider;
	enum rpmem_persist_method persist_method;
	int (*persist)(const void *addr, size_t len);
	void *(*memcpy_persist)(void *pmemdest, const void *src, size_t len);
	int (*deep_persist)(const void *addr, size_t len, void *ctx);
	void *ctx;
};

struct rpmemd_fip *rpmemd_fip_init(const char *node,
		const char *service,
		struct rpmemd_fip_attr *attr,
		struct rpmem_resp_attr *resp,
		enum rpmem_err *err);
void rpmemd_fip_fini(struct rpmemd_fip *fip);

int rpmemd_fip_accept(struct rpmemd_fip *fip, int timeout);
int rpmemd_fip_process_start(struct rpmemd_fip *fip);
int rpmemd_fip_process_stop(struct rpmemd_fip *fip);
int rpmemd_fip_wait_close(struct rpmemd_fip *fip, int timeout);
int rpmemd_fip_close(struct rpmemd_fip *fip);
