/*
 * Copyright 2016, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * rpmem_fip_common.h -- common definitions for librpmem and rpmemd
 */

#include <string.h>
#include <netinet/in.h>

#include <rdma/fabric.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_rma.h>

#define RPMEM_FIVERSION FI_VERSION(1, 1)
#define RPMEM_FIP_CQ_WAIT_MS	100

#define min(a, b) ((a) < (b) ? (a) : (b))

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

int rpmem_fip_read_eq(struct fid_eq *eq, struct fi_eq_cm_entry *entry,
	uint32_t exp_event, fid_t exp_fid, int timeout);

size_t rpmem_fip_cq_size(size_t nlanes, enum rpmem_persist_method pm,
	enum rpmem_fip_node node);

size_t rpmem_fip_max_nlanes(struct fi_info *fi, enum rpmem_persist_method pm,
	enum rpmem_fip_node node);
