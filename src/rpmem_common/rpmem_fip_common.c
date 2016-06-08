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
 * rpmem_common.c -- common definitions for librpmem and rpmemd
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>

#include "rpmem_common.h"
#include "rpmem_fip_common.h"
#include "rpmem_proto.h"

#include "rpmem_common_log.h"

#include <rdma/fi_errno.h>

/*
 * rpmem_fip_get_hints -- return fabric interface information hints
 */
struct fi_info *
rpmem_fip_get_hints(enum rpmem_provider provider)
{
	RPMEMC_ASSERT(provider < MAX_RPMEM_PROV);

	struct fi_info *hints = fi_allocinfo();
	if (!hints) {
		RPMEMC_LOG(ERR, "!fi_allocinfo");
		return NULL;
	}

	/* connection-oriented endpoint */
	hints->ep_attr->type = FI_EP_MSG;

	/*
	 * Basic memory registration mode indicates that MR attributes
	 * (rkey, lkey) are selected by provider.
	 */
	hints->domain_attr->mr_mode = FI_MR_BASIC;

	/*
	 * FI_THREAD_SAFE indicates MT applications can access any
	 * resources through interface without any restrictions
	 */
	hints->domain_attr->threading = FI_THREAD_SAFE;

	/*
	 * FI_MSG - SEND and RECV
	 * FI_RMA - WRITE and READ
	 */
	hints->caps = FI_MSG | FI_RMA;

	/* must register locally accessed buffers */
	hints->mode = FI_CONTEXT | FI_LOCAL_MR | FI_RX_CQ_DATA;

	/* READ-after-WRITE and SEND-after-WRITE message ordering required */
	hints->tx_attr->msg_order = FI_ORDER_RAW | FI_ORDER_SAW;

	/* IPv4 address format */
	hints->addr_format = FI_SOCKADDR_IN;

	if (provider != RPMEM_PROV_UNKNOWN) {
		const char *prov_name = rpmem_provider_to_str(provider);
		RPMEMC_ASSERT(prov_name != NULL);

		hints->fabric_attr->prov_name = strdup(prov_name);
		if (!hints->fabric_attr->prov_name) {
			RPMEMC_LOG(ERR, "!strdup(provider)");
			goto err_strdup;
		}
	}

	return hints;
err_strdup:
	fi_freeinfo(hints);
	return NULL;
}

/*
 * rpmem_fip_probe_get -- return list of available providers
 */
int
rpmem_fip_probe_get(const char *target, struct rpmem_fip_probe *probe)
{
	struct fi_info *hints = rpmem_fip_get_hints(RPMEM_PROV_UNKNOWN);
	if (!hints)
		return -1;

	int ret;
	struct fi_info *fi;
	ret = fi_getinfo(RPMEM_FIVERSION, target, NULL, 0, hints, &fi);
	if (ret) {
		goto err_getinfo;
	}

	if (probe) {
		memset(probe, 0, sizeof(*probe));

		struct fi_info *prov = fi;
		while (prov) {
			enum rpmem_provider p = rpmem_provider_from_str(
					prov->fabric_attr->prov_name);
			if (p == RPMEM_PROV_UNKNOWN)
				continue;

			probe->providers |= (1U << p);
			prov = prov->next;
		}
	}

	fi_freeinfo(fi);
err_getinfo:
	fi_freeinfo(hints);
	return ret;
}

/*
 * rpmem_fip_read_eq -- read event queue entry and expect specified event
 * and fid
 *
 * Returns:
 * 1 - timeout
 * 0 - success
 * otherwise - error
 */
int
rpmem_fip_read_eq(struct fid_eq *eq, struct fi_eq_cm_entry *entry,
	uint32_t exp_event, fid_t exp_fid, int timeout)
{
	int ret;
	ssize_t sret;
	uint32_t event;
	struct fi_eq_err_entry err;

	sret = fi_eq_sread(eq, &event, entry, sizeof(*entry), -1, 0);
	if (timeout != -1 && sret == -FI_ETIMEDOUT)
		return 1;

	if (sret < 0 || (size_t)sret != sizeof(*entry)) {
		ret = (int)sret;

		sret = fi_eq_readerr(eq, &err, 0);
		if (sret < 0) {
			RPMEMC_LOG(ERR, "error reading from event queue: "
				"cannot read error from event queue: %s",
				fi_strerror((int)sret));
		} else {
			RPMEMC_LOG(ERR, "error reading from event queue: %s",
					fi_eq_strerror(eq, err.prov_errno,
						NULL, NULL, 0));
		}

		goto err_read_event;
	}

	if (event != exp_event || entry->fid != exp_fid) {
		RPMEMC_LOG(ERR, "unexpected event received (%u) "
				"expected (%u)%s", event, exp_event,
				entry->fid != exp_fid ?
				" invalid endpoint" : "");
		ret = -1;
		goto err_event_data;
	}

	return 0;
err_event_data:
err_read_event:

	return ret;
}

/*
 * rpmem_fip_lane_attr -- lane attributes
 *
 * This structure describes how many SQ, RQ and CQ entries are
 * required for a single lane.
 *
 * NOTE:
 * - WRITE, READ and SEND requests are placed in SQ,
 * - RECV requests are placed in RQ.
 */
struct rpmem_fip_lane_attr {
	size_t n_per_sq; /* number of entries per lane in send queue  */
	size_t n_per_rq; /* number of entries per lane in receive queue */
	size_t n_per_cq; /* number of entries per lane in completion queue */
};

static struct rpmem_fip_lane_attr
rpmem_fip_lane_attrs[MAX_RPMEM_FIP_NODE][MAX_RPMEM_PM] = {
	[RPMEM_FIP_NODE_CLIENT][RPMEM_PM_GPSPM] = {
		.n_per_sq = 2, /* WRITE + SEND */
		.n_per_rq = 1, /* RECV */
		.n_per_cq = 2, /* SEND + RECV */
	},
	[RPMEM_FIP_NODE_CLIENT][RPMEM_PM_APM] = {
		.n_per_sq = 2, /* WRITE + READ */
		.n_per_rq = 0, /* unused */
		.n_per_cq = 1, /* READ */
	},
	[RPMEM_FIP_NODE_SERVER][RPMEM_PM_GPSPM] = {
		.n_per_sq = 1, /* SEND */
		.n_per_rq = 1, /* RECV */
		.n_per_cq = 2, /* SEND + RECV */
	},
	[RPMEM_FIP_NODE_SERVER][RPMEM_PM_APM] = {
		.n_per_sq = 0, /* unused */
		.n_per_rq = 0, /* unused */
		.n_per_cq = 0, /* unused */
	},
};

/*
 * rpmem_fip_cq_size -- returns completion queue size based on number
 * of lanes, persist method and node type
 */
size_t
rpmem_fip_cq_size(size_t nlanes, enum rpmem_persist_method pm,
	enum rpmem_fip_node node)
{
	RPMEMC_ASSERT(pm < MAX_RPMEM_PM);
	RPMEMC_ASSERT(node < MAX_RPMEM_FIP_NODE);

	struct rpmem_fip_lane_attr *attr = &rpmem_fip_lane_attrs[node][pm];
	size_t cq_mul = attr->n_per_cq ? : 1;

	return nlanes * cq_mul;
}

/*
 * rpmem_fip_max_nlanes -- returns maximum number of lanes based on
 * fabric interface information, persist method and node type.
 */
size_t
rpmem_fip_max_nlanes(struct fi_info *fi, enum rpmem_persist_method pm,
	enum rpmem_fip_node node)
{
	RPMEMC_ASSERT(pm < MAX_RPMEM_PM);
	RPMEMC_ASSERT(node < MAX_RPMEM_FIP_NODE);

	struct rpmem_fip_lane_attr *attr = &rpmem_fip_lane_attrs[node][pm];

	size_t sq_size = fi->tx_attr->size;
	size_t rq_size = fi->rx_attr->size;

	size_t sq_div = attr->n_per_sq ? : 1;
	size_t rq_div = attr->n_per_rq ? : 1;

	size_t max_by_sq = sq_size / sq_div;
	size_t max_by_rq = rq_size / rq_div;

	return min(max_by_sq, max_by_rq);
}
