/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2016-2020, Intel Corporation */

/*
 * rpmem_fip_common.h -- common definitions for librpmem and rpmemd
 */

#ifndef RPMEM_FIP_COMMON_H
#define RPMEM_FIP_COMMON_H 1

#include <string.h>
#include <netinet/in.h>

#include <rdma/fabric.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_rma.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RPMEM_FIVERSION FI_VERSION(1, 4)
#define RPMEM_FIP_CQ_WAIT_MS	100

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

/*
 * rpmem_fip_node -- client or server node type
 */
enum rpmem_fip_node {
	RPMEM_FIP_NODE_CLIENT,
	RPMEM_FIP_NODE_SERVER,

	MAX_RPMEM_FIP_NODE,
};

/*
 * rpmem_fip_probe -- list of providers
 */
struct rpmem_fip_probe {
	unsigned providers;
	size_t max_wq_size[MAX_RPMEM_PROV];
};

/*
 * rpmem_fip_probe -- returns true if specified provider is available
 */
static inline int
rpmem_fip_probe(struct rpmem_fip_probe probe, enum rpmem_provider provider)
{
	return (probe.providers & (1U << provider)) != 0;
}

/*
 * rpmem_fip_probe_any -- returns true if any provider is available
 */
static inline int
rpmem_fip_probe_any(struct rpmem_fip_probe probe)
{
	return probe.providers != 0;
}

int rpmem_fip_probe_get(const char *target, struct rpmem_fip_probe *probe);

struct fi_info *rpmem_fip_get_hints(enum rpmem_provider provider);

int rpmem_fip_read_eq_check(struct fid_eq *eq, struct fi_eq_cm_entry *entry,
	uint32_t exp_event, fid_t exp_fid, int timeout);

int rpmem_fip_read_eq(struct fid_eq *eq, struct fi_eq_cm_entry *entry,
		uint32_t *event, int timeout);

size_t rpmem_fip_cq_size(enum rpmem_persist_method pm,
	enum rpmem_fip_node node);
size_t rpmem_fip_wq_size(enum rpmem_persist_method pm,
	enum rpmem_fip_node node);
size_t rpmem_fip_rx_size(enum rpmem_persist_method pm,
	enum rpmem_fip_node node);

size_t rpmem_fip_max_nlanes(struct fi_info *fi);

void rpmem_fip_print_info(struct fi_info *fi);

#ifdef __cplusplus
}
#endif

#endif
