// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2019, Intel Corporation */

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

#include "valgrind_internal.h"

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

	hints->addr_format = FI_SOCKADDR;

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
			if (p == RPMEM_PROV_UNKNOWN) {
				prov = prov->next;
				continue;
			}

			probe->providers |= (1U << p);
			probe->max_wq_size[p] = prov->tx_attr->size;
			prov = prov->next;
		}
	}

	fi_freeinfo(fi);
err_getinfo:
	fi_freeinfo(hints);
	return ret;
}

/*
 * rpmem_fip_read_eq -- read event queue entry with specified timeout
 */
int
rpmem_fip_read_eq(struct fid_eq *eq, struct fi_eq_cm_entry *entry,
	uint32_t *event, int timeout)
{
	int ret;
	ssize_t sret;
	struct fi_eq_err_entry err;

	sret = fi_eq_sread(eq, event, entry, sizeof(*entry), timeout, 0);
	VALGRIND_DO_MAKE_MEM_DEFINED(&sret, sizeof(sret));

	if (timeout != -1 && (sret == -FI_ETIMEDOUT || sret == -FI_EAGAIN)) {
		errno = ETIMEDOUT;
		return 1;
	}

	if (sret < 0 || (size_t)sret != sizeof(*entry)) {
		if (sret < 0)
			ret = (int)sret;
		else
			ret = -1;

		sret = fi_eq_readerr(eq, &err, 0);
		if (sret < 0) {
			errno = EIO;
			RPMEMC_LOG(ERR, "error reading from event queue: "
				"cannot read error from event queue: %s",
				fi_strerror((int)sret));
		} else if (sret > 0) {
			RPMEMC_ASSERT(sret == sizeof(err));
			errno = -err.prov_errno;
			RPMEMC_LOG(ERR, "error reading from event queue: %s",
					fi_eq_strerror(eq, err.prov_errno,
						NULL, NULL, 0));
		}

		return ret;
	}

	return 0;
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
rpmem_fip_read_eq_check(struct fid_eq *eq, struct fi_eq_cm_entry *entry,
	uint32_t exp_event, fid_t exp_fid, int timeout)
{
	uint32_t event;
	int ret = rpmem_fip_read_eq(eq, entry, &event, timeout);
	if (ret)
		return ret;

	if (event != exp_event || entry->fid != exp_fid) {
		errno = EIO;
		RPMEMC_LOG(ERR, "unexpected event received (%u) "
				"expected (%u)%s", event, exp_event,
				entry->fid != exp_fid ?
				" invalid endpoint" : "");

		return -1;
	}

	return 0;
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

/* queues size required by remote persist operation methods */
static const struct rpmem_fip_lane_attr
rpmem_fip_lane_attrs[MAX_RPMEM_FIP_NODE][MAX_RPMEM_PM] = {
	[RPMEM_FIP_NODE_CLIENT][RPMEM_PM_GPSPM] = {
		.n_per_sq = 2, /* WRITE + SEND */
		.n_per_rq = 1, /* RECV */
		.n_per_cq = 3,
	},
	[RPMEM_FIP_NODE_CLIENT][RPMEM_PM_APM] = {
		/* WRITE + READ for persist, WRITE + SEND for deep persist */
		.n_per_sq = 2, /* WRITE + SEND */
		.n_per_rq = 1, /* RECV */
		.n_per_cq = 3,
	},
	[RPMEM_FIP_NODE_SERVER][RPMEM_PM_GPSPM] = {
		.n_per_sq = 1, /* SEND */
		.n_per_rq = 1, /* RECV */
		.n_per_cq = 3,
	},
	[RPMEM_FIP_NODE_SERVER][RPMEM_PM_APM] = {
		.n_per_sq = 1, /* SEND */
		.n_per_rq = 1, /* RECV */
		.n_per_cq = 3,
	},
};

/*
 * rpmem_fip_cq_size -- returns completion queue size based on
 * persist method and node type
 */
size_t
rpmem_fip_cq_size(enum rpmem_persist_method pm, enum rpmem_fip_node node)
{
	RPMEMC_ASSERT(pm < MAX_RPMEM_PM);
	RPMEMC_ASSERT(node < MAX_RPMEM_FIP_NODE);

	const struct rpmem_fip_lane_attr *attr =
			&rpmem_fip_lane_attrs[node][pm];
	return attr->n_per_cq ? : 1;
}

/*
 * rpmem_fip_wq_size -- returns submission queue (transmit queue) size based
 * on persist method and node type
 */
size_t
rpmem_fip_wq_size(enum rpmem_persist_method pm, enum rpmem_fip_node node)
{
	RPMEMC_ASSERT(pm < MAX_RPMEM_PM);
	RPMEMC_ASSERT(node < MAX_RPMEM_FIP_NODE);

	const struct rpmem_fip_lane_attr *attr =
			&rpmem_fip_lane_attrs[node][pm];
	return attr->n_per_sq ? : 1;
}

/*
 * rpmem_fip_rx_size -- returns receive queue size based
 * on persist method and node type
 */
size_t
rpmem_fip_rx_size(enum rpmem_persist_method pm, enum rpmem_fip_node node)
{
	RPMEMC_ASSERT(pm < MAX_RPMEM_PM);
	RPMEMC_ASSERT(node < MAX_RPMEM_FIP_NODE);

	const struct rpmem_fip_lane_attr *attr =
			&rpmem_fip_lane_attrs[node][pm];
	return attr->n_per_rq ? : 1;
}

/*
 * rpmem_fip_max_nlanes -- returns maximum number of lanes
 */
size_t
rpmem_fip_max_nlanes(struct fi_info *fi)
{
	return min(min(fi->domain_attr->tx_ctx_cnt,
			fi->domain_attr->rx_ctx_cnt),
			fi->domain_attr->cq_cnt);
}

/*
 * rpmem_fip_print_info -- print some useful info about fabric interface
 */
void
rpmem_fip_print_info(struct fi_info *fi)
{
	RPMEMC_LOG(INFO, "libfabric version: %s",
			fi_tostr(fi, FI_TYPE_VERSION));

	char *str = fi_tostr(fi, FI_TYPE_INFO);
	char *buff = strdup(str);
	if (!buff) {
		RPMEMC_LOG(ERR, "!allocating string buffer for "
				"libfabric interface information");
		return;
	}

	RPMEMC_LOG(INFO, "libfabric interface info:");

	char *nl;
	char *last = buff;
	while (last != NULL) {
		nl = strchr(last, '\n');
		if (nl) {
			*nl = '\0';
			nl++;
		}

		RPMEMC_LOG(INFO, "%s", last);

		last = nl;
	}

	free(buff);
}
