/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2016-2020, Intel Corporation */

/*
 * rpmem_fip.h -- rpmem libfabric provider module header file
 */

#ifndef RPMEM_FIP_H
#define RPMEM_FIP_H

#include <stdint.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

struct rpmem_fip;

struct rpmem_fip_attr {
	enum rpmem_provider provider;
	size_t max_wq_size;
	enum rpmem_persist_method persist_method;
	void *laddr;
	size_t size;
	size_t buff_size;
	unsigned nlanes;
	void *raddr;
	uint64_t rkey;
};

struct rpmem_fip *rpmem_fip_init(const char *node, const char *service,
		struct rpmem_fip_attr *attr, unsigned *nlanes);
void rpmem_fip_fini(struct rpmem_fip *fip);

int rpmem_fip_connect(struct rpmem_fip *fip);
int rpmem_fip_close(struct rpmem_fip *fip);
int rpmem_fip_process_start(struct rpmem_fip *fip);
int rpmem_fip_process_stop(struct rpmem_fip *fip);

int rpmem_fip_flush(struct rpmem_fip *fip, size_t offset, size_t len,
		unsigned lane, unsigned flags);

int rpmem_fip_drain(struct rpmem_fip *fip, unsigned lane);

int rpmem_fip_persist(struct rpmem_fip *fip, size_t offset, size_t len,
		unsigned lane, unsigned flags);

int rpmem_fip_read(struct rpmem_fip *fip, void *buff,
		size_t len, size_t off, unsigned lane);
void rpmem_fip_probe_fork_safety(void);

size_t rpmem_fip_get_wq_size(struct rpmem_fip *fip);

#ifdef __cplusplus
}
#endif

#endif
