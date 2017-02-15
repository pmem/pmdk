/*
 * Copyright 2016-2017, Intel Corporation
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
 * rpmem_fip.c -- rpmem libfabric provider module source file
 */

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <limits.h>

#include <rdma/fabric.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_errno.h>
#include <rdma/fi_rma.h>

#include "out.h"
#include "util.h"
#include "rpmem_common.h"
#include "rpmem_fip_common.h"
#include "rpmem_proto.h"
#include "rpmem_util.h"
#include "rpmem_fip_msg.h"
#include "rpmem_fip.h"
#include "valgrind_internal.h"

#define RPMEM_FI_ERR(e, fmt, args...)\
	ERR(fmt ": %s", ## args, fi_strerror((e)))

#define RPMEM_FI_CLOSE(f, fmt, args...) (\
{\
	int oerrno = errno;\
	int ret = fi_close(&(f)->fid);\
	if (ret)\
		RPMEM_FI_ERR(ret, fmt, ## args);\
	errno = oerrno;\
	ret;\
})

#define LANE_UNION_ALIGN_SIZE 64
#define LANE_UNION_ALIGN __attribute__((aligned(LANE_UNION_ALIGN_SIZE)))

#define RPMEM_RAW_BUFF_SIZE 4096
#define RPMEM_RAW_SIZE 8

typedef int (*rpmem_fip_persist_fn)(struct rpmem_fip *fip, size_t offset,
		size_t len, unsigned lane);

typedef int (*rpmem_fip_process_fn)(struct rpmem_fip *fip,
		void *context, uint64_t flags);

typedef int (*rpmem_fip_init_fn)(struct rpmem_fip *fip);
typedef void (*rpmem_fip_fini_fn)(struct rpmem_fip *fip);

/*
 * rpmem_fip_ops -- operations specific for persistency method
 */
struct rpmem_fip_ops {
	rpmem_fip_persist_fn persist;
	rpmem_fip_process_fn process;
	rpmem_fip_init_fn lanes_init;
	rpmem_fip_fini_fn lanes_fini;
	rpmem_fip_init_fn lanes_post;
};

/*
 * rpmem_fip_lane -- base lane structure
 */
struct rpmem_fip_lane {
	struct fid_ep *ep;		/* endpoint */
	struct fid_cq *cq;		/* completion queue */
	uint64_t event;
};

/*
 * rpmem_fip_plane_apm -- persist operation's lane for APM
 */
struct rpmem_fip_plane_apm {
	struct rpmem_fip_lane base;	/* base lane structure */
	struct rpmem_fip_rma write;	/* WRITE message */
	struct rpmem_fip_rma read;	/* READ message */
};

/*
 * rpmem_fip_plane_gpspm -- persist operation's lane for GPSPM
 */
struct rpmem_fip_plane_gpspm {
	struct rpmem_fip_lane base;	/* base lane structure */
	struct rpmem_fip_rma write;	/* WRITE message */
	struct rpmem_fip_msg send;	/* SEND message */
	struct rpmem_fip_msg recv;	/* RECV message */

};

/*
 * rpmem_fip_rlane -- read operation's lane
 */
struct rpmem_fip_rlane {
	struct rpmem_fip_lane base;	/* base lane structure */
	struct rpmem_fip_rma read;	/* READ message */
};

struct rpmem_fip {
	struct fi_info *fi; /* fabric interface information */
	struct fid_fabric *fabric; /* fabric domain */
	struct fid_domain *domain; /* fabric protection domain */
	struct fid_eq *eq; /* event queue */

	volatile int closing; /* closing connections in progress */

	size_t cq_size;	/* completion queue size */

	uint64_t raddr;	/* remote memory base address */
	uint64_t rkey;	/* remote memory protection key */
	void *laddr;	/* local memory base address */
	size_t size;	/* memory size */
	struct fid_mr *mr; /* local memory region */
	void *mr_desc;	/* local memory descriptor */

	enum rpmem_persist_method persist_method;
	struct rpmem_fip_ops *ops;

	unsigned nlanes;
	union {
		struct rpmem_fip_lane base;
		struct rpmem_fip_plane_apm apm;
		struct rpmem_fip_plane_gpspm gpspm;
	} LANE_UNION_ALIGN *lanes;


	struct rpmem_msg_persist *pmsg;	/* persist message buffer */
	struct fid_mr *pmsg_mr;		/* persist message memory region */
	void *pmsg_mr_desc;		/* persist message memory descriptor */

	struct rpmem_msg_persist_resp *pres; /* persist response buffer */
	struct fid_mr *pres_mr;		/* persist response memory region */
	void *pres_mr_desc;		/* persist response memory descriptor */

	void *raw_buff;			/* READ-after-WRITE buffer */
	struct fid_mr *raw_mr;		/* RAW memory region */
	void *raw_mr_desc;		/* RAW memory descriptor */
};

/*
 * rpmem_fip_lane_begin -- (internal) intialize list of events for lane
 */
static inline void
rpmem_fip_lane_begin(struct rpmem_fip_lane *lanep, uint64_t event)
{
	lanep->event = event;
}

/*
 * rpmem_fip_lane_init -- (internal) initialize single lane
 */
static int
rpmem_fip_lane_init(struct rpmem_fip *fip, struct rpmem_fip_lane *lanep)
{
	int ret;

	struct fi_cq_attr cq_attr = {
		.size = fip->cq_size,
		.flags = 0,
		.format = FI_CQ_FORMAT_MSG,
		.wait_obj = FI_WAIT_UNSPEC,
		.signaling_vector = 0,
		.wait_cond = FI_CQ_COND_NONE,
		.wait_set = NULL,
	};

	/* create a completion queue */
	ret = fi_cq_open(fip->domain, &cq_attr, &lanep->cq, NULL);
	if (ret) {
		RPMEM_FI_ERR(ret, "opening completion queue");
		goto err_cq_open;
	}

	/* create an endpoint */
	ret = fi_endpoint(fip->domain, fip->fi, &lanep->ep, NULL);
	if (ret) {
		RPMEM_FI_ERR(ret, "allocating endpoint");
		goto err_endpoint;
	}

	/*
	 * Bind an event queue to an endpoint to get
	 * connection-related events for the endpoint.
	 */
	ret = fi_ep_bind(lanep->ep, &fip->eq->fid, 0);
	if (ret) {
		RPMEM_FI_ERR(ret, "binding event queue to endpoint");
		goto err_ep_bind_eq;
	}

	/*
	 * Bind a completion queue to an endpoint to get completion
	 * events of specified inbound/outbound operations.
	 *
	 * FI_SELECTIVE_COMPLETION means all inbound/outbound operations
	 * must explicitly specify if the completion event should be
	 * generated or not using FI_COMPLETION flag.
	 *
	 * The completion events received are highly related to the
	 * persistency method used and are configured in lanes
	 * initialization specified for persistency method utilized.
	 */
	ret = fi_ep_bind(lanep->ep, &lanep->cq->fid,
			FI_RECV | FI_TRANSMIT | FI_SELECTIVE_COMPLETION);
	if (ret) {
		RPMEM_FI_ERR(ret, "binding completion queue to endpoint");
		goto err_ep_bind_cq;
	}

	/*
	 * Enable endpoint so it is possible to post inbound/outbound
	 * operations if required.
	 */
	ret = fi_enable(lanep->ep);
	if (ret) {
		RPMEM_FI_ERR(ret, "activating endpoint");
		goto err_fi_enable;
	}

	return 0;
err_fi_enable:
err_ep_bind_cq:
err_ep_bind_eq:
err_endpoint:
	RPMEM_FI_CLOSE(lanep->cq, "closing completion queue");
err_cq_open:
	return -1;
}

/*
 * rpmem_fip_lane_fini -- (internal) deinitialize single lane
 */
static int
rpmem_fip_lane_fini(struct rpmem_fip_lane *lanep)
{
	int ret;
	int lret = 0;

	ret = RPMEM_FI_CLOSE(lanep->ep, "closing endpoint");
	if (ret)
		lret = ret;

	ret = RPMEM_FI_CLOSE(lanep->cq, "closing completion queue");
	if (ret)
		lret = ret;


	return lret;
}

/*
 * rpmem_fip_lane_wait -- (internal) wait for specific event on completion queue
 */
static int
rpmem_fip_lane_wait(struct rpmem_fip *fip, struct rpmem_fip_lane *lanep,
	uint64_t e)
{
	ssize_t sret = 0;
	struct fi_cq_err_entry err;
	const char *str_err;
	int ret = 0;
	struct fi_cq_msg_entry cq_entry;

	while (!fip->closing && (lanep->event & e)) {
		sret = fi_cq_sread(lanep->cq, &cq_entry, 1, NULL, -1);
		if (unlikely(fip->closing))
			return 0;

		if (unlikely(sret == -FI_EAGAIN) || sret == 0)
			continue;

		if (unlikely(sret < 0)) {
			ret = (int)sret;
			goto err_cq_read;
		}

		lanep->event &= ~cq_entry.flags;
	}

	return 0;
err_cq_read:
	sret = fi_cq_readerr(lanep->cq, &err, 0);
	if (sret < 0) {
		RPMEM_FI_ERR((int)sret, "error reading from completion queue: "
			"cannot read error from event queue");
		goto err;
	}

	str_err = fi_cq_strerror(lanep->cq, err.prov_errno, NULL, NULL, 0);
	RPMEM_LOG(ERR, "error reading from completion queue: %s", str_err);
err:
	return ret;
}

/*
 * rpmem_fip_set_nlanes -- (internal) set maximum number of lanes supported
 */
static void
rpmem_fip_set_nlanes(struct rpmem_fip *fip, unsigned nlanes)
{
	size_t max_nlanes = rpmem_fip_max_nlanes(fip->fi);
	RPMEM_ASSERT(max_nlanes < UINT_MAX);

	fip->nlanes = min((unsigned)max_nlanes, nlanes);
}

/*
 * rpmem_fip_getinfo -- (internal) get fabric interface information
 */
static int
rpmem_fip_getinfo(struct rpmem_fip *fip, const char *node, const char *service,
	enum rpmem_provider provider, enum rpmem_persist_method pm)
{
	int ret = 0;
	struct fi_info *hints = rpmem_fip_get_hints(provider);
	if (!hints) {
		RPMEM_LOG(ERR, "!getting fabric interface information hints");
		goto err_hints;
	}

	hints->tx_attr->size = rpmem_fip_tx_size(pm, RPMEM_FIP_NODE_CLIENT);
	hints->rx_attr->size = rpmem_fip_rx_size(pm, RPMEM_FIP_NODE_CLIENT);

	ret = fi_getinfo(RPMEM_FIVERSION, node, service,
			0, hints, &fip->fi);
	if (ret) {
		RPMEM_FI_ERR(ret, "getting fabric interface information");
		goto err_fi_getinfo;
	}

	rpmem_fip_print_info(fip->fi);

	/* fallback to free the hints */
err_fi_getinfo:
	fi_freeinfo(hints);
err_hints:
	return ret;
}

/*
 * rpmem_fip_init_fabric_res -- (internal) initialize common fabric resources
 */
static int
rpmem_fip_init_fabric_res(struct rpmem_fip *fip)
{
	int ret;
	ret = fi_fabric(fip->fi->fabric_attr, &fip->fabric, NULL);
	if (ret) {
		RPMEM_FI_ERR(ret, "opening fabric domain");
		goto err_fi_fabric;
	}

	ret = fi_domain(fip->fabric, fip->fi, &fip->domain, NULL);
	if (ret) {
		RPMEM_FI_ERR(ret, "opening fabric access domain");
		goto err_fi_domain;
	}

	struct fi_eq_attr eq_attr = {
		.size = 0, /* use default value */
		.flags = 0,
		.wait_obj = FI_WAIT_UNSPEC,
		.signaling_vector = 0,
		.wait_set = NULL,
	};

	ret = fi_eq_open(fip->fabric, &eq_attr, &fip->eq, NULL);
	if (ret) {
		RPMEM_FI_ERR(ret, "opening event queue");
		goto err_eq_open;
	}

	return 0;
err_eq_open:
	RPMEM_FI_CLOSE(fip->domain, "closing fabric access domain");
err_fi_domain:
	RPMEM_FI_CLOSE(fip->fabric, "closing fabric domain");
err_fi_fabric:
	return ret;
}

/*
 * rpmem_fip_fini_fabric_res -- (internal) deinitialize common fabric resources
 */
static void
rpmem_fip_fini_fabric_res(struct rpmem_fip *fip)
{
	RPMEM_FI_CLOSE(fip->eq, "closing event queue");
	RPMEM_FI_CLOSE(fip->domain, "closing fabric access domain");
	RPMEM_FI_CLOSE(fip->fabric, "closing fabric domain");
}

/*
 * rpmem_fip_init_memory -- (internal) initialize common memory resources
 */
static int
rpmem_fip_init_memory(struct rpmem_fip *fip)
{
	ASSERTne(Pagesize, 0);
	int ret;

	/*
	 * Register local memory space. The local memory will be used
	 * with WRITE operation in rpmem_fip_persist function thus
	 * the FI_WRITE access flag.
	 */
	ret = fi_mr_reg(fip->domain, fip->laddr, fip->size,
			FI_WRITE, 0, 0, 0, &fip->mr, NULL);
	if (ret) {
		RPMEM_FI_ERR(ret, "registrating memory");
		return ret;
	}

	/* get local memory descriptor */
	fip->mr_desc = fi_mr_desc(fip->mr);

	return 0;
}

/*
 * rpmem_fip_fini_memory -- (internal) deinitialize common memory resources
 */
static void
rpmem_fip_fini_memory(struct rpmem_fip *fip)
{
	RPMEM_FI_CLOSE(fip->mr, "unregistering memory");
}

/*
 * rpmem_fip_lanes_init_common -- (internal) initialize common lanes resources
 */
static int
rpmem_fip_lanes_init_common(struct rpmem_fip *fip)
{
	int ret;

	fip->lanes = calloc(fip->nlanes, sizeof(*fip->lanes));
	if (!fip->lanes) {
		RPMEM_LOG(ERR, "!allocating lanes");
		goto err_alloc_lanes;
	}

	unsigned i;
	for (i = 0; i < fip->nlanes; i++) {
		ret = rpmem_fip_lane_init(fip, &fip->lanes[i].base);
		if (ret)
			goto err_lane_init;

	}

	return 0;
err_lane_init:
	for (unsigned j = 0; j < i; j++)
		rpmem_fip_lane_fini(&fip->lanes[i].base);
	free(fip->lanes);
err_alloc_lanes:
	return -1;
}

/*
 * rpmem_fip_lanes_fini_common -- (internal) deinitialize common lanes
 * resrouces
 */
static int
rpmem_fip_lanes_fini_common(struct rpmem_fip *fip)
{
	int lret = 0;
	int ret;

	for (unsigned i = 0; i < fip->nlanes; i++) {
		ret = rpmem_fip_lane_fini(&fip->lanes[i].base);
		if (ret)
			lret = ret;
	}

	free(fip->lanes);

	return lret;
}

/*
 * rpmem_fip_lanes_init -- (internal) initialize lanes
 */
static int
rpmem_fip_lanes_init(struct rpmem_fip *fip)
{
	int ret;

	ret = rpmem_fip_lanes_init_common(fip);
	if (ret)
		return ret;

	ret = fip->ops->lanes_init(fip);
	if (ret)
		goto err_init_lanes;

	return 0;
err_init_lanes:
	rpmem_fip_lanes_fini_common(fip);
	return ret;
}

/*
 * rpmem_fip_lane_connect -- (internal) connect on a single lane
 */
static int
rpmem_fip_lane_connect(struct rpmem_fip *fip, struct rpmem_fip_lane *lanep)
{
	struct fi_eq_cm_entry entry;
	int ret;

	ret = fi_connect(lanep->ep, fip->fi->dest_addr, NULL, 0);
	if (ret) {
		RPMEM_FI_ERR(ret, "initiating connection request");
		return ret;
	}

	return rpmem_fip_read_eq_check(fip->eq, &entry, FI_CONNECTED,
			&lanep->ep->fid,
			RPMEM_CONNECT_TIMEOUT);
}

/*
 * rpmem_fip_lanes_connect -- (internal) establish connections on all lanes
 */
static int
rpmem_fip_lanes_connect(struct rpmem_fip *fip)
{
	int ret;

	for (unsigned i = 0; i < fip->nlanes; i++) {
		struct rpmem_fip_lane *lanep = &fip->lanes[i].base;
		ret = rpmem_fip_lane_connect(fip, lanep);
		if (ret)
			return ret;
	}

	return 0;
}

/*
 * rpmem_fip_init_lanes_apm -- (internal) initialize lanes for APM
 */
static int
rpmem_fip_init_lanes_apm(struct rpmem_fip *fip)
{
	ASSERTne(Pagesize, 0);
	int ret;

	ASSERT(IS_PAGE_ALIGNED(RPMEM_RAW_BUFF_SIZE));
	errno = posix_memalign((void **)&fip->raw_buff, Pagesize,
			RPMEM_RAW_BUFF_SIZE);
	if (errno) {
		RPMEM_LOG(ERR, "!allocating APM RAW buffer");
		goto err_malloc_raw;
	}

	/* register read-after-write buffer */
	ret = fi_mr_reg(fip->domain, fip->raw_buff, RPMEM_RAW_BUFF_SIZE,
			FI_REMOTE_WRITE, 0, 0, 0, &fip->raw_mr, NULL);
	if (ret) {
		RPMEM_FI_ERR(ret, "registering APM read buffer");
		goto err_fi_raw_mr;
	}

	/* get read-after-write buffer local descriptor */
	fip->raw_mr_desc = fi_mr_desc(fip->raw_mr);

	/*
	 * Initialize all required structures for:
	 * WRITE and READ operations.
	 *
	 * If the completion is required the FI_COMPLETION flag and
	 * appropriate context should be used.
	 *
	 * In APM only the READ completion is required.
	 * The context is a lane structure.
	 */
	for (unsigned i = 0; i < fip->nlanes; i++) {

		/* WRITE */
		rpmem_fip_rma_init(&fip->lanes[i].apm.write,
				fip->mr_desc, 0,
				fip->rkey,
				&fip->lanes[i].apm,
				0);

		/* READ */
		rpmem_fip_rma_init(&fip->lanes[i].apm.read,
				fip->raw_mr_desc, 0,
				fip->rkey,
				&fip->lanes[i].apm,
				FI_COMPLETION);
	}

	return 0;
err_fi_raw_mr:
	free(fip->raw_buff);
err_malloc_raw:
	return -1;
}

/*
 * rpmem_fip_fini_lanes_apm -- (internal) deinitialize lanes for APM
 */
static void
rpmem_fip_fini_lanes_apm(struct rpmem_fip *fip)
{
	RPMEM_FI_CLOSE(fip->raw_mr, "unregistering APM read buffer");
	free(fip->raw_buff);
}

/*
 * rpmem_fip_post_lanes_apm -- (internal) post APM-related buffers
 */
static int
rpmem_fip_post_lanes_apm(struct rpmem_fip *fip)
{
	/* nothing to do */
	return 0;
}

/*
 * rpmem_fip_persist_apm -- (internal) perform persist operation for APM
 */
static int
rpmem_fip_persist_apm(struct rpmem_fip *fip, size_t offset,
	size_t len, unsigned lane)
{
	struct rpmem_fip_plane_apm *lanep = &fip->lanes[lane].apm;

	int ret;
	void *laddr = (void *)((uintptr_t)fip->laddr + offset);
	uint64_t raddr = fip->raddr + offset;

	rpmem_fip_lane_begin(&lanep->base, FI_READ);

	/* WRITE for requested memory region */
	ret = rpmem_fip_writemsg(lanep->base.ep,
			&lanep->write, laddr, len, raddr);
	if (unlikely(ret)) {
		RPMEM_FI_ERR(ret, "RMA write");
		return ret;
	}

	/* READ to read-after-write buffer */
	ret = rpmem_fip_readmsg(lanep->base.ep, &lanep->read, fip->raw_buff,
			RPMEM_RAW_SIZE, raddr);
	if (unlikely(ret)) {
		RPMEM_FI_ERR(ret, "RMA read");
		return ret;
	}

	/* wait for READ completion */
	ret = rpmem_fip_lane_wait(fip, &lanep->base, FI_READ);
	if (unlikely(ret)) {
		ERR("waiting for READ completion failed");
		return ret;
	}

	return ret;
}

/*
 * rpmem_fip_gpspm_post_resp -- (internal) post persist response message buffer
 */
static inline int
rpmem_fip_gpspm_post_resp(struct rpmem_fip *fip,
	struct rpmem_fip_plane_gpspm *lanep)
{
	int ret = rpmem_fip_recvmsg(lanep->base.ep, &lanep->recv);
	if (unlikely(ret)) {
		RPMEM_FI_ERR(ret, "posting GPSPM recv buffer");
		return ret;
	}

	return 0;
}

/*
 * rpmem_fip_post_lanes_gpspm -- (internal) post all persist response message
 * buffers
 */
static int
rpmem_fip_post_lanes_gpspm(struct rpmem_fip *fip)
{
	int ret = 0;
	for (unsigned i = 0; i < fip->nlanes; i++) {
		ret = rpmem_fip_gpspm_post_resp(fip, &fip->lanes[i].gpspm);
		if (ret)
			break;
	}

	return ret;
}

/*
 * rpmem_fip_init_lanes_gpspm -- (internal) initialize lanes for GPSPM
 */
static int
rpmem_fip_init_lanes_gpspm(struct rpmem_fip *fip)
{
	ASSERTne(Pagesize, 0);

	int ret = 0;

	/* allocate persist messages buffer */
	size_t msg_size = fip->nlanes * sizeof(struct rpmem_msg_persist);
	msg_size = PAGE_ALIGNED_UP_SIZE(msg_size);
	errno = posix_memalign((void **)&fip->pmsg, Pagesize, msg_size);
	if (errno) {
		RPMEM_LOG(ERR, "!allocating messages buffer");
		ret = -1;
		goto err_malloc_pmsg;
	}

	/*
	 * Register persist messages buffer. The persist messages
	 * are sent to daemon thus the FI_SEND access flag.
	 */
	ret = fi_mr_reg(fip->domain, fip->pmsg, msg_size, FI_SEND,
			0, 0, 0, &fip->pmsg_mr, NULL);
	if (ret) {
		RPMEM_FI_ERR(ret, "registering messages buffer");
		goto err_fi_mr_reg_pmsg;
	}

	/* get persist messages buffer local descriptor */
	fip->pmsg_mr_desc = fi_mr_desc(fip->pmsg_mr);

	/* allocate persist response messages buffer */
	size_t msg_resp_size = fip->nlanes *
				sizeof(struct rpmem_msg_persist_resp);
	msg_resp_size = PAGE_ALIGNED_UP_SIZE(msg_resp_size);
	errno = posix_memalign((void **)&fip->pres, Pagesize, msg_resp_size);
	if (errno) {
		RPMEM_LOG(ERR, "!allocating messages response buffer");
		ret = -1;
		goto err_malloc_pres;
	}

	/*
	 * Register persist messages response buffer. The persist response
	 * messages are received from daemon thus the FI_RECV access flag.
	 */
	ret = fi_mr_reg(fip->domain, fip->pres, msg_resp_size, FI_RECV,
			0, 0, 0, &fip->pres_mr, NULL);
	if (ret) {
		RPMEM_FI_ERR(ret, "registering messages response buffer");
		goto err_fi_mr_reg_pres;
	}

	/* get persist response messages buffer local descriptor */
	fip->pres_mr_desc = fi_mr_desc(fip->pres_mr);

	/*
	 * Initialize all required structures for:
	 * WRITE, SEND and RECV operations.
	 *
	 * If the completion is required the FI_COMPLETION flag and
	 * appropriate context should be used.
	 *
	 * In GPSPM only the RECV and SEND completions are required.
	 *
	 * For RECV the context is RECV operation structure used for
	 * fi_recvmsg(3) function call.
	 *
	 * For SEND the context is lane structure.
	 *
	 * The received buffer contains a lane id which is used
	 * to obtain a lane which must be signaled that operation
	 * has been completed.
	 */
	unsigned i;
	for (i = 0; i < fip->nlanes; i++) {
		/* WRITE */
		rpmem_fip_rma_init(&fip->lanes[i].gpspm.write,
				fip->mr_desc, 0,
				fip->rkey,
				&fip->lanes[i].gpspm,
				0);

		/* SEND */
		rpmem_fip_msg_init(&fip->lanes[i].gpspm.send,
				fip->pmsg_mr_desc, 0,
				&fip->lanes[i].gpspm,
				&fip->pmsg[i],
				sizeof(fip->pmsg[i]),
				FI_COMPLETION);

		/* RECV */
		rpmem_fip_msg_init(&fip->lanes[i].gpspm.recv,
				fip->pres_mr_desc, 0,
				&fip->lanes[i].gpspm.recv,
				&fip->pres[i],
				sizeof(fip->pres[i]),
				FI_COMPLETION);
	}

	return 0;
err_fi_mr_reg_pres:
	free(fip->pres);
err_malloc_pres:
	RPMEM_FI_CLOSE(fip->pmsg_mr, "unregistering messages buffer");
err_fi_mr_reg_pmsg:
	free(fip->pmsg);
err_malloc_pmsg:
	return ret;
}

/*
 * rpmem_fip_fini_lanes_gpspm -- (internal) deinitialize lanes for GPSPM
 */
static void
rpmem_fip_fini_lanes_gpspm(struct rpmem_fip *fip)
{
	RPMEM_FI_CLOSE(fip->pmsg_mr, "unregistering messages buffer");
	RPMEM_FI_CLOSE(fip->pres_mr, "unregistering messages "
			"response buffer");
	free(fip->pmsg);
	free(fip->pres);
}

/*
 * rpmem_fip_persist_gpspm -- (internal) perform persist operation for GPSPM
 */
static int
rpmem_fip_persist_gpspm(struct rpmem_fip *fip, size_t offset,
	size_t len, unsigned lane)
{
	struct rpmem_fip_plane_gpspm *lanep = &fip->lanes[lane].gpspm;
	void *laddr = (void *)((uintptr_t)fip->laddr + offset);
	uint64_t raddr = fip->raddr + offset;
	struct rpmem_msg_persist *msg;
	struct rpmem_fip_plane_gpspm *gpspm = (void *)lanep;
	int ret;

	ret = rpmem_fip_lane_wait(fip, &lanep->base, FI_SEND);
	if (unlikely(ret)) {
		ERR("waiting for SEND completion failed");
		return ret;
	}

	rpmem_fip_lane_begin(&lanep->base, FI_RECV | FI_SEND);

	/* WRITE for requested memory region */
	ret = rpmem_fip_writemsg(lanep->base.ep,
			&gpspm->write, laddr, len, raddr);
	if (unlikely(ret)) {
		RPMEM_FI_ERR((int)ret, "RMA write");
		return ret;
	}

	/* SEND persist message */
	msg = rpmem_fip_msg_get_pmsg(&gpspm->send);
	msg->lane = lane;
	msg->addr = raddr;
	msg->size = len;

	ret = rpmem_fip_sendmsg(lanep->base.ep, &gpspm->send);
	if (unlikely(ret)) {
		RPMEM_FI_ERR(ret, "MSG send");
		return ret;
	}

	/* wait for persist operation completion */
	ret = rpmem_fip_lane_wait(fip, &lanep->base, FI_RECV);
	if (unlikely(ret)) {
		ERR("waiting for RECV completion failed");
		return ret;
	}

	ret = rpmem_fip_gpspm_post_resp(fip, lanep);
	if (unlikely(ret)) {
		ERR("posting RECV buffer failed");
		return ret;
	}

	return 0;
}

/*
 * rpmem_fip_ops -- some operations specific for persistency method used
 */
static struct rpmem_fip_ops rpmem_fip_ops[MAX_RPMEM_PM] = {
	[RPMEM_PM_GPSPM] = {
		.persist = rpmem_fip_persist_gpspm,
		.lanes_init = rpmem_fip_init_lanes_gpspm,
		.lanes_fini = rpmem_fip_fini_lanes_gpspm,
		.lanes_post = rpmem_fip_post_lanes_gpspm,
	},
	[RPMEM_PM_APM] = {
		.persist = rpmem_fip_persist_apm,
		.lanes_init = rpmem_fip_init_lanes_apm,
		.lanes_fini = rpmem_fip_fini_lanes_apm,
		.lanes_post = rpmem_fip_post_lanes_apm,
	},
};

/*
 * rpmem_fip_set_attr -- (internal) set required attributes
 */
static void
rpmem_fip_set_attr(struct rpmem_fip *fip, struct rpmem_fip_attr *attr)
{
	fip->raddr = (uint64_t)attr->raddr;
	fip->rkey = attr->rkey;
	fip->laddr = attr->laddr;
	fip->size = attr->size;
	fip->persist_method = attr->persist_method;

	rpmem_fip_set_nlanes(fip, attr->nlanes);

	/* one for read operation */
	fip->cq_size = rpmem_fip_cq_size(fip->persist_method,
			RPMEM_FIP_NODE_CLIENT);

	fip->ops = &rpmem_fip_ops[fip->persist_method];
}

/*
 * rpmem_fip_init -- initialize fabric provider
 */
struct rpmem_fip *
rpmem_fip_init(const char *node, const char *service,
	struct rpmem_fip_attr *attr, unsigned *nlanes)
{
	int ret;

	struct rpmem_fip *fip = calloc(1, sizeof(*fip));
	if (!fip) {
		RPMEM_LOG(ERR, "!allocating fabric handle");
		return NULL;
	}

	ret = rpmem_fip_getinfo(fip, node, service,
		attr->provider, attr->persist_method);
	if (ret)
		goto err_getinfo;

	rpmem_fip_set_attr(fip, attr);

	*nlanes = fip->nlanes;

	ret = rpmem_fip_init_fabric_res(fip);
	if (ret)
		goto err_init_fabric_res;

	ret = rpmem_fip_init_memory(fip);
	if (ret)
		goto err_init_memory;

	ret = rpmem_fip_lanes_init(fip);
	if (ret)
		goto err_init_lanes;

	return fip;
err_init_lanes:
	rpmem_fip_fini_memory(fip);
err_init_memory:
	rpmem_fip_fini_fabric_res(fip);
err_init_fabric_res:
	fi_freeinfo(fip->fi);
err_getinfo:
	free(fip);
	return NULL;
}

/*
 * rpmem_fip_fini -- deinitialize fabric provider
 */
void
rpmem_fip_fini(struct rpmem_fip *fip)
{
	fip->ops->lanes_fini(fip);
	rpmem_fip_lanes_fini_common(fip);
	rpmem_fip_fini_memory(fip);
	rpmem_fip_fini_fabric_res(fip);
	fi_freeinfo(fip->fi);
	free(fip);
}

/*
 * rpmem_fip_connect -- connect to remote peer
 */
int
rpmem_fip_connect(struct rpmem_fip *fip)
{
	int ret;

	ret = fip->ops->lanes_post(fip);
	if (ret)
		goto err_lanes_post;

	ret = rpmem_fip_lanes_connect(fip);
	if (ret)
		goto err_lanes_connect;

	return 0;
err_lanes_connect:
err_lanes_post:
	return ret;
}

/*
 * rpmem_fip_close -- close connection to remote peer
 */
int
rpmem_fip_close(struct rpmem_fip *fip)
{
	int ret;
	int lret = 0;
	for (unsigned i = 0; i < fip->nlanes; i++) {
		ret = fi_shutdown(fip->lanes[i].base.ep, 0);
		if (ret) {
			RPMEM_FI_ERR(ret, "disconnecting endpoint");
			lret = ret;
		}
	}

	return lret;
}

/*
 * rpmem_fip_persist -- perform remote persist operation
 */
int
rpmem_fip_persist(struct rpmem_fip *fip, size_t offset, size_t len,
	unsigned lane)
{
	RPMEM_ASSERT(lane < fip->nlanes);
	if (unlikely(lane >= fip->nlanes))
		return EINVAL; /* it will be passed to errno */

	if (unlikely(offset + len > fip->size))
		return EINVAL; /* it will be passed to errno */

	if (unlikely(len == 0)) {
		return 0;
	}


	int ret = 0;
	while (len > 0) {
		size_t tmp_len = len < fip->fi->ep_attr->max_msg_size ?
			len : fip->fi->ep_attr->max_msg_size;

		ret = fip->ops->persist(fip, offset, tmp_len, lane);
		if (ret) {
			RPMEM_LOG(ERR, "persist operation failed");
			goto err;
		}

		offset += tmp_len;
		len -= tmp_len;
	}
err:
	return ret;
}

/*
 * rpmem_fip_read -- perform read operation
 */
int
rpmem_fip_read(struct rpmem_fip *fip, void *buff, size_t len,
	size_t off, unsigned lane)
{
	int ret;
	RPMEM_ASSERT(lane < fip->nlanes);
	if (unlikely(lane >= fip->nlanes))
		return EINVAL; /* it will be passed to errno */

	size_t rd_buff_len = len < fip->fi->ep_attr->max_msg_size ?
		len : fip->fi->ep_attr->max_msg_size;

	void *rd_buff;		/* buffer for read operation */
	struct fid_mr *rd_mr;	/* read buffer memory region */
	void *rd_mr_desc;	/* read buffer memory descriptor */
	struct rpmem_fip_rlane rd_lane;

	/* allocate buffer for read operation */
	errno = posix_memalign((void **)&rd_buff, Pagesize,
			rd_buff_len);
	if (errno) {
		RPMEM_LOG(ERR, "!allocating read buffer");
		ret = errno;
		goto err_malloc_rd_buff;
	}

	/*
	 * Register buffer for read operation.
	 * The read operation utilizes READ operation thus
	 * the FI_REMOTE_WRITE flag.
	 */
	ret = fi_mr_reg(fip->domain, rd_buff,
			rd_buff_len, FI_REMOTE_WRITE,
			0, 0, 0, &rd_mr, NULL);
	if (ret) {
		RPMEM_FI_ERR(ret, "registrating read buffer");
		goto err_rd_mr;
	}

	/* get read buffer local memory descriptor */
	rd_mr_desc = fi_mr_desc(rd_mr);

	/*
	 * Initialize READ message. The completion is required in order
	 * to signal thread that READ operation has been completed.
	 */
	rpmem_fip_rma_init(&rd_lane.read, rd_mr_desc, 0,
			fip->rkey, &rd_lane, FI_COMPLETION);

	size_t rd = 0;
	uint8_t *cbuff = buff;
	struct rpmem_fip_lane *lanep = &fip->lanes[lane].base;

	while (rd < len) {
		size_t rd_len = len - rd < rd_buff_len ?
				len - rd : rd_buff_len;
		size_t rd_off = off + rd;
		uint64_t raddr = fip->raddr + rd_off;

		rpmem_fip_lane_begin(lanep, FI_READ);

		ret = rpmem_fip_readmsg(lanep->ep, &rd_lane.read,
				rd_buff, rd_len, raddr);
		if (ret) {
			RPMEM_FI_ERR(ret, "RMA read");
			goto err_readmsg;
		}

		VALGRIND_DO_MAKE_MEM_DEFINED(rd_buff, rd_len);

		ret = rpmem_fip_lane_wait(fip, lanep, FI_READ);
		if (ret) {
			ERR("error when processing read request");
			goto err_lane_wait;
		}

		memcpy(&cbuff[rd], rd_buff, rd_len);

		rd += rd_len;
	}

	ret = 0;
err_lane_wait:
err_readmsg:
	RPMEM_FI_CLOSE(rd_mr, "unregistering memory");
err_rd_mr:
	free(rd_buff);
err_malloc_rd_buff:
	return ret;
}

/*
 * parse_bool -- convert string value to boolean
 */
static int
parse_bool(const char *str_value)
{
	if (strcmp(str_value, "0") == 0 ||
			strcasecmp(str_value, "false") == 0 ||
			strcasecmp(str_value, "no") == 0 ||
			strcasecmp(str_value, "off") == 0) {
		return 0;
	}

	if (strcmp(str_value, "1") == 0 ||
			strcasecmp(str_value, "true") == 0 ||
			strcasecmp(str_value, "yes") == 0 ||
			strcasecmp(str_value, "on") == 0) {
		return 1;
	}

	return -1;
}

/*
 * rpmem_fip_param_get -- read environment variable in the libfabric way
 *
 * - If parameter does not exist the output value is not changed.
 * - If the environment variable is not set the output value is not changed.
 * - If the environment variable is set and its value is not correct the output
 * value is set to error value.
 * - If the environment variable is set and its value is correct the output
 * value is set according to the environment variable value.
 */
static void
rpmem_fip_param_get(const char *var_name, int *value)
{
	struct fi_param *params;
	int count;
	int ret = fi_getparams(&params, &count);
	if (ret != FI_SUCCESS) {
		RPMEM_FI_ERR(ret, "getting fabric parameters list");
		return;
	}

	for (int i = 0; i < count; ++i) {
		if (strcmp(params[i].name, var_name) != 0)
			continue;
		if (!params[i].value) {
			break;
		}
		*value = parse_bool(params[i].value);
		break;
	}

	fi_freeparams(params);
}

#define LIBFABRIC_FORK_UNSAFE_VAR "FI_FORK_UNSAFE"

/*
 * rpmem_fip_probe_fork_safety -- probe if libfabric is fork safe
 */
void
rpmem_fip_probe_fork_safety(int *fork_unsafe)
{
	fork_unsafe = 0; /* false by default */
	rpmem_fip_param_get(LIBFABRIC_FORK_UNSAFE_VAR, fork_unsafe);
}
