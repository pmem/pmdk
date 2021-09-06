// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2021, Intel Corporation */

/*
 * rpmemd_fip.c -- rpmemd libfabric provider module source file
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <rdma/fabric.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_errno.h>

#include "rpmemd_log.h"

#include "rpmem_common.h"
#include "rpmem_proto.h"
#include "rpmem_fip_msg.h"
#include "rpmem_fip_common.h"
#include "rpmemd_fip.h"

#include "os_thread.h"
#include "util.h"
#include "valgrind_internal.h"

#define RPMEMD_FI_ERR(e, fmt, args...)\
	RPMEMD_LOG(ERR, fmt ": %s", ## args, fi_strerror((e)))

#define RPMEMD_FI_CLOSE(f, fmt, args...) (\
{\
	int ret = fi_close(&(f)->fid);\
	if (ret)\
		RPMEMD_FI_ERR(ret, fmt, ## args);\
	ret;\
})

/*
 * rpmem_fip_lane -- base lane structure
 */
struct rpmem_fip_lane {
	struct fid_ep *ep;
	struct fid_cq *cq;
};

/*
 * rpmemd_fip_lane -- daemon's lane
 */
struct rpmemd_fip_lane {
	struct rpmem_fip_lane base;	/* lane base structure */
	struct rpmem_fip_msg recv;	/* RECV message */
	struct rpmem_fip_msg send;	/* SEND message */
	struct rpmem_msg_persist_resp resp; /* persist response msg buffer */
	int send_posted;		/* send buffer has been posted */
	int recv_posted;		/* recv buffer has been posted */
};

/*
 * rpmemd_fip_thread -- thread context
 */
struct rpmemd_fip_thread {
	struct rpmemd_fip *fip;		/* main context */
	os_thread_t thread;		/* thread structure */
	struct fid_cq *cq;		/* per-thread completion queue */
	struct rpmemd_fip_lane **lanes; /* lanes processed by this thread */
	size_t nlanes;	/* number of lanes processed by this thread */
};

/*
 * rpmemd_fip -- main context of rpmemd_fip
 */
struct rpmemd_fip {
	struct fi_info *fi;		/* fabric interface information */
	struct fid_fabric *fabric;	/* fabric domain */
	struct fid_domain *domain;	/* fabric protection domain */
	struct fid_eq *eq;		/* event queue */
	struct fid_pep *pep;		/* passive endpoint - listener */
	struct fid_mr *mr;		/* memory region for pool */

	int (*persist)(const void *addr, size_t len);	/* persist function */
	void *(*memcpy_persist)(void *pmemdest, const void *src, size_t len);
	int (*deep_persist)(const void *addr, size_t len, void *ctx);
	void *ctx;
	void *addr;			/* pool's address */
	size_t size;			/* size of the pool */
	enum rpmem_persist_method persist_method;

	volatile int closing;	/* flag for closing background threads */
	unsigned nlanes;	/* number of lanes */
	size_t nthreads;	/* number of threads for processing */
	size_t cq_size;	/* size of completion queue */
	size_t lanes_per_thread; /* number of lanes per thread */
	size_t buff_size;	/* size of buffer for inlined data */

	struct rpmemd_fip_lane *lanes;
	struct rpmem_fip_lane rd_lane; /* lane for read operation */

	void *pmsg;			/* persist message buffer */
	size_t pmsg_size; /* persist message buffer size including alignment */
	struct fid_mr *pmsg_mr;		/* persist message memory region */
	void *pmsg_mr_desc;		/* persist message local descriptor */

	struct rpmem_msg_persist_resp *pres; /* persist response buffer */
	struct fid_mr *pres_mr;		/* persist response memory region */
	void *pres_mr_desc;		/* persist response local descriptor */

	struct rpmemd_fip_thread *threads;
};

/*
 * rpmemd_fip_get_pmsg -- return persist message buffer
 */
static inline struct rpmem_msg_persist *
rpmemd_fip_get_pmsg(struct rpmemd_fip *fip, size_t idx)
{
	return (struct rpmem_msg_persist *)
		((uintptr_t)fip->pmsg + idx * fip->pmsg_size);
}

/*
 * rpmemd_fip_getinfo -- obtain fabric interface information
 */
static int
rpmemd_fip_getinfo(struct rpmemd_fip *fip, const char *service,
	const char *node, enum rpmem_provider provider)
{
	int ret;

	struct fi_info *hints = rpmem_fip_get_hints(provider);
	if (!hints) {
		RPMEMD_LOG(ERR, "getting fabric interface hints");
		ret = -1;
		goto err_fi_get_hints;
	}

	ret = fi_getinfo(RPMEM_FIVERSION, node, service, FI_SOURCE,
			hints, &fip->fi);
	if (ret) {
		RPMEMD_FI_ERR(ret, "getting fabric interface information");
		goto err_fi_getinfo;
	}

	rpmem_fip_print_info(fip->fi);

	fi_freeinfo(hints);
	return 0;
err_fi_getinfo:
	fi_freeinfo(hints);
err_fi_get_hints:
	return ret;
}

/*
 * rpmemd_fip_set_resp -- fill the response structure
 */
static int
rpmemd_fip_set_resp(struct rpmemd_fip *fip, struct rpmem_resp_attr *resp)
{
	int ret;
	if (fip->fi->addr_format == FI_SOCKADDR_IN) {
		struct sockaddr_in addr_in;
		size_t addrlen = sizeof(addr_in);

		ret = fi_getname(&fip->pep->fid, &addr_in, &addrlen);
		if (ret) {
			RPMEMD_FI_ERR(ret, "getting local endpoint address");
			goto err_fi_getname;
		}

		if (!addr_in.sin_port) {
			RPMEMD_LOG(ERR, "dynamic allocation of port failed");
			goto err_port;
		}

		resp->port = htons(addr_in.sin_port);
	} else if (fip->fi->addr_format == FI_SOCKADDR_IN6) {
		struct sockaddr_in6 addr_in6;
		size_t addrlen = sizeof(addr_in6);

		ret = fi_getname(&fip->pep->fid, &addr_in6, &addrlen);
		if (ret) {
			RPMEMD_FI_ERR(ret, "getting local endpoint address");
			goto err_fi_getname;
		}

		if (!addr_in6.sin6_port) {
			RPMEMD_LOG(ERR, "dynamic allocation of port failed");
			goto err_port;
		}

		resp->port = htons(addr_in6.sin6_port);
	} else {
		RPMEMD_LOG(ERR, "invalid address format");
		return -1;
	}

	resp->rkey = fi_mr_key(fip->mr);
	resp->persist_method = fip->persist_method;
	resp->raddr = (uint64_t)fip->addr;
	resp->nlanes = fip->nlanes;

	return 0;
err_port:
err_fi_getname:
	return -1;
}

/*
 * rpmemd_fip_init_fabric_res -- initialize common fabric's resources
 */
static int
rpmemd_fip_init_fabric_res(struct rpmemd_fip *fip)
{
	int ret;
	ret = fi_fabric(fip->fi->fabric_attr, &fip->fabric, NULL);
	if (ret) {
		RPMEMD_FI_ERR(ret, "opening fabric domain");
		goto err_fi_fabric;
	}

	ret = fi_domain(fip->fabric, fip->fi, &fip->domain, NULL);
	if (ret) {
		RPMEMD_FI_ERR(ret, "opening fabric access domain");
		goto err_fi_domain;
	}

	struct fi_eq_attr eq_attr = {
		.size = 0,	/* use default */
		.flags = 0,
		.wait_obj = FI_WAIT_UNSPEC,
		.signaling_vector = 0,
		.wait_set = NULL,
	};

	ret = fi_eq_open(fip->fabric, &eq_attr, &fip->eq, NULL);
	if (ret) {
		RPMEMD_FI_ERR(ret, "opening event queue");
		goto err_eq_open;
	}

	ret = fi_passive_ep(fip->fabric, fip->fi, &fip->pep, NULL);
	if (ret) {
		RPMEMD_FI_ERR(ret, "allocating passive endpoint");
		goto err_pep;
	}

	ret = fi_pep_bind(fip->pep, &fip->eq->fid, 0);
	if (ret) {
		RPMEMD_FI_ERR(ret, "binding event queue to passive endpoint");
		goto err_pep_bind_eq;
	}

	return 0;
err_pep_bind_eq:
	RPMEMD_FI_CLOSE(fip->pep, "closing passive endpoint");
err_pep:
	RPMEMD_FI_CLOSE(fip->eq, "closing event queue");
err_eq_open:
	RPMEMD_FI_CLOSE(fip->domain, "closing fabric access domain");
err_fi_domain:
	RPMEMD_FI_CLOSE(fip->fabric, "closing fabric domain");
err_fi_fabric:
	return ret;
}

/*
 * rpmemd_fip_fini_fabric_res -- deinitialize common fabric resources
 */
static void
rpmemd_fip_fini_fabric_res(struct rpmemd_fip *fip)
{
	RPMEMD_FI_CLOSE(fip->pep, "closing passive endpoint");
	RPMEMD_FI_CLOSE(fip->eq, "closing event queue");
	RPMEMD_FI_CLOSE(fip->domain, "closing fabric access domain");
	RPMEMD_FI_CLOSE(fip->fabric, "closing fabric domain");
}

/*
 * rpmemd_fip_init_memory -- initialize memory pool's resources
 */
static int
rpmemd_fip_init_memory(struct rpmemd_fip *fip)
{
	int ret;

	/*
	 * Register memory region with appropriate access bits:
	 * - FI_REMOTE_READ  - remote peer can issue READ operation,
	 * - FI_REMOTE_WRITE - remote peer can issue WRITE operation,
	 */
	ret = fi_mr_reg(fip->domain, fip->addr, fip->size,
			FI_REMOTE_READ | FI_REMOTE_WRITE, 0, 0, 0,
			&fip->mr, NULL);
	if (ret) {
		RPMEMD_FI_ERR(ret, "registering memory");
		return -1;
	}

	return 0;
}

/*
 * rpmemd_fip_fini_memory -- deinitialize memory pool's resources
 */
static void
rpmemd_fip_fini_memory(struct rpmemd_fip *fip)
{
	RPMEMD_FI_CLOSE(fip->mr, "unregistering memory");
}

/*
 * rpmemd_fip_init_ep -- initialize active endpoint
 */
static int
rpmemd_fip_init_ep(struct rpmemd_fip *fip, struct fi_info *info,
	struct rpmem_fip_lane *lanep)
{
	int ret;

	info->tx_attr->size = rpmem_fip_wq_size(fip->persist_method,
			RPMEM_FIP_NODE_SERVER);

	info->rx_attr->size = rpmem_fip_rx_size(fip->persist_method,
			RPMEM_FIP_NODE_SERVER);

	/* create an endpoint from fabric interface info */
	ret = fi_endpoint(fip->domain, info, &lanep->ep, NULL);
	if (ret) {
		RPMEMD_FI_ERR(ret, "allocating endpoint");
		goto err_endpoint;
	}

	/* bind event queue to the endpoint */
	ret = fi_ep_bind(lanep->ep, &fip->eq->fid, 0);
	if (ret) {
		RPMEMD_FI_ERR(ret, "binding event queue to endpoint");
		goto err_bind_eq;
	}

	/*
	 * Bind completion queue to the endpoint.
	 * Use a single completion queue for outbound and inbound work
	 * requests. Use selective completion implies adding FI_COMPLETE
	 * flag to each WR which needs a completion.
	 */
	ret = fi_ep_bind(lanep->ep, &lanep->cq->fid,
			FI_RECV | FI_TRANSMIT | FI_SELECTIVE_COMPLETION);
	if (ret) {
		RPMEMD_FI_ERR(ret, "binding completion queue to endpoint");
		goto err_bind_cq;
	}

	/* enable the endpoint */
	ret = fi_enable(lanep->ep);
	if (ret) {
		RPMEMD_FI_ERR(ret, "enabling endpoint");
		goto err_enable;
	}

	return 0;
err_enable:
err_bind_cq:
err_bind_eq:
	RPMEMD_FI_CLOSE(lanep->ep, "closing endpoint");
err_endpoint:
	return -1;
}

/*
 * rpmemd_fip_fini_ep -- close endpoint
 */
static int
rpmemd_fip_fini_ep(struct rpmem_fip_lane *lanep)
{
	return RPMEMD_FI_CLOSE(lanep->ep, "closing endpoint");
}

/*
 * rpmemd_fip_post_msg -- post RECV buffer
 */
static inline int
rpmemd_fip_post_msg(struct rpmemd_fip_lane *lanep)
{
	int ret = rpmem_fip_recvmsg(lanep->base.ep, &lanep->recv);
	if (ret) {
		RPMEMD_FI_ERR(ret, "posting recv buffer");
		return ret;
	}

	lanep->recv_posted = 1;

	return 0;
}

/*
 * rpmemd_fip_post_resp -- post SEND buffer
 */
static inline int
rpmemd_fip_post_resp(struct rpmemd_fip_lane *lanep)
{
	int ret = rpmem_fip_sendmsg(lanep->base.ep, &lanep->send,
			sizeof(struct rpmem_msg_persist_resp));
	if (ret) {
		RPMEMD_FI_ERR(ret, "posting send buffer");
		return ret;
	}

	lanep->send_posted = 1;

	return 0;
}

/*
 * rpmemd_fip_post_common -- post all RECV messages
 */
static int
rpmemd_fip_post_common(struct rpmemd_fip *fip, struct rpmemd_fip_lane *lanep)
{
	int ret = rpmem_fip_recvmsg(lanep->base.ep, &lanep->recv);
	if (ret) {
		RPMEMD_FI_ERR(ret, "posting recv buffer");
		return ret;
	}

	lanep->recv_posted = 1;

	return 0;
}

/*
 * rpmemd_fip_lanes_init -- initialize all lanes
 */
static int
rpmemd_fip_lanes_init(struct rpmemd_fip *fip)
{

	fip->lanes = calloc(fip->nlanes, sizeof(*fip->lanes));
	if (!fip->lanes) {
		RPMEMD_ERR("!allocating lanes");
		goto err_alloc;
	}

	return 0;
err_alloc:
	return -1;
}

/*
 * rpmemd_fip_fini_lanes -- deinitialize all lanes
 */
static void
rpmemd_fip_fini_lanes(struct rpmemd_fip *fip)
{
	free(fip->lanes);
}

/*
 * rpmemd_fip_init_common -- initialize common resources
 */
static int
rpmemd_fip_init_common(struct rpmemd_fip *fip)
{
	int ret;

	/* allocate persist message buffer */
	size_t msg_size = fip->nlanes * fip->pmsg_size;
	fip->pmsg = malloc(msg_size);
	if (!fip->pmsg) {
		RPMEMD_LOG(ERR, "!allocating messages buffer");
		goto err_msg_malloc;
	}

	/* register persist message buffer */
	ret = fi_mr_reg(fip->domain, fip->pmsg, msg_size, FI_RECV,
			0, 0, 0, &fip->pmsg_mr, NULL);
	if (ret) {
		RPMEMD_FI_ERR(ret, "registering messages buffer");
		goto err_mr_reg_msg;
	}

	/* get persist message buffer's local descriptor */
	fip->pmsg_mr_desc = fi_mr_desc(fip->pmsg_mr);

	/* allocate persist response message buffer */
	size_t msg_resp_size = fip->nlanes *
		sizeof(struct rpmem_msg_persist_resp);
	fip->pres = malloc(msg_resp_size);
	if (!fip->pres) {
		RPMEMD_FI_ERR(ret, "allocating messages response buffer");
		goto err_msg_resp_malloc;
	}

	/* register persist response message buffer */
	ret = fi_mr_reg(fip->domain, fip->pres, msg_resp_size, FI_SEND,
			0, 0, 0, &fip->pres_mr, NULL);
	if (ret) {
		RPMEMD_FI_ERR(ret, "registering messages "
				"response buffer");
		goto err_mr_reg_msg_resp;
	}

	/* get persist message buffer's local descriptor */
	fip->pres_mr_desc = fi_mr_desc(fip->pres_mr);

	/* initialize lanes */
	unsigned i;
	for (i = 0; i < fip->nlanes; i++) {
		struct rpmemd_fip_lane *lanep = &fip->lanes[i];

		/* initialize RECV message */
		rpmem_fip_msg_init(&lanep->recv,
				fip->pmsg_mr_desc, 0,
				lanep,
				rpmemd_fip_get_pmsg(fip, i),
				fip->pmsg_size,
				FI_COMPLETION);

		/* initialize SEND message */
		rpmem_fip_msg_init(&lanep->send,
				fip->pres_mr_desc, 0,
				lanep,
				&fip->pres[i],
				sizeof(fip->pres[i]),
				FI_COMPLETION);
	}

	return 0;
err_mr_reg_msg_resp:
	free(fip->pres);
err_msg_resp_malloc:
	RPMEMD_FI_CLOSE(fip->pmsg_mr,
			"unregistering messages buffer");
err_mr_reg_msg:
	free(fip->pmsg);
err_msg_malloc:
	return -1;
}

/*
 * rpmemd_fip_fini_common -- deinitialize common resources and return last
 * error code
 */
static int
rpmemd_fip_fini_common(struct rpmemd_fip *fip)
{
	int lret = 0;
	int ret;

	ret = RPMEMD_FI_CLOSE(fip->pmsg_mr,
			"unregistering messages buffer");
	if (ret)
		lret = ret;

	ret = RPMEMD_FI_CLOSE(fip->pres_mr,
			"unregistering messages response buffer");
	if (ret)
		lret = ret;

	free(fip->pmsg);
	free(fip->pres);

	return lret;
}

/*
 * rpmemd_fip_check_pmsg -- verify persist message
 */
static inline int
rpmemd_fip_check_pmsg(struct rpmemd_fip *fip, struct rpmem_msg_persist *pmsg)
{
	if (pmsg->lane >= fip->nlanes) {
		RPMEMD_LOG(ERR, "invalid lane number -- %u", pmsg->lane);
		return -1;
	}

	uintptr_t raddr = pmsg->addr;
	uintptr_t laddr = (uintptr_t)fip->addr;

	if (raddr < laddr || raddr + pmsg->size > laddr + fip->size) {
		RPMEMD_LOG(ERR, "invalid address or size requested "
			"for persist operation (0x%lx, %lu)",
			raddr, pmsg->size);
		return -1;
	}

	return 0;
}

/*
 * rpmemd_fip_process_send -- process FI_SEND completion
 */
static int
rpmemd_fip_process_send(struct rpmemd_fip *fip, struct rpmemd_fip_lane *lanep)
{
	lanep->send_posted = 0;

	if (lanep->recv_posted)
		return 0;

	struct rpmem_msg_persist_resp *pres =
		rpmem_fip_msg_get_pres(&lanep->send);

	*pres = lanep->resp;

	int ret;

	/* post lane's RECV buffer */
	ret = rpmemd_fip_post_msg(lanep);
	if (unlikely(ret))
		goto err;

	/* post lane's SEND buffer */
	ret = rpmemd_fip_post_resp(lanep);
err:
	return ret;
}

/*
 * rpmemd_fip_process_recv -- process FI_RECV completion
 */
static int
rpmemd_fip_process_recv(struct rpmemd_fip *fip, struct rpmemd_fip_lane *lanep)
{
	int ret = 0;

	lanep->recv_posted = 0;

	/*
	 * Get persist message and persist message response from appropriate
	 * buffers. The persist message is in lane's RECV buffer and the
	 * persist response message in lane's SEND buffer.
	 */
	struct rpmem_msg_persist *pmsg = rpmem_fip_msg_get_pmsg(&lanep->recv);
	VALGRIND_DO_MAKE_MEM_DEFINED(pmsg, sizeof(*pmsg));

	/* verify persist message */
	ret = rpmemd_fip_check_pmsg(fip, pmsg);
	if (unlikely(ret))
		goto err;
	unsigned mode = pmsg->flags & RPMEM_FLUSH_PERSIST_MASK;

	if (mode == RPMEM_DEEP_PERSIST) {
		fip->deep_persist((void *)pmsg->addr, pmsg->size, fip->ctx);
	} else if (mode == RPMEM_PERSIST_SEND) {
		fip->memcpy_persist((void *)pmsg->addr, pmsg->data, pmsg->size);
	} else {
		fip->persist((void *)pmsg->addr, pmsg->size);
	}

	struct rpmem_msg_persist_resp *pres = lanep->send_posted ?
		&lanep->resp : rpmem_fip_msg_get_pres(&lanep->send);

	/* return back the lane id */
	pres->lane = pmsg->lane;

	if (!lanep->send_posted) {
		/* post lane's RECV buffer */
		ret = rpmemd_fip_post_msg(lanep);
		if (unlikely(ret))
			goto err;

		/* post lane's SEND buffer */
		ret = rpmemd_fip_post_resp(lanep);
	}

err:
	return ret;
}

/*
 * rpmemd_fip_cq_read -- wait for specific events on completion queue
 */
static int
rpmemd_fip_cq_read(struct rpmemd_fip *fip, struct fid_cq *cq,
	struct rpmemd_fip_lane **lanep, uint64_t *event, uint64_t event_mask)
{
	struct fi_cq_err_entry err;
	struct fi_cq_msg_entry cq_entry;
	const char *str_err;
	ssize_t sret;
	int ret;

	while (!fip->closing) {
		sret = fi_cq_sread(cq, &cq_entry, 1, NULL,
				RPMEM_FIP_CQ_WAIT_MS);

		if (unlikely(fip->closing))
			break;

		if (unlikely(sret == -FI_EAGAIN || sret == 0))
			continue;

		if (unlikely(sret < 0)) {
			ret = (int)sret;
			goto err_cq_read;
		}

		if (!(cq_entry.flags & event_mask)) {
			RPMEMD_LOG(ERR, "unexpected event received %lx",
					cq_entry.flags);
			ret = -1;
			goto err;
		}

		if (!cq_entry.op_context) {
			RPMEMD_LOG(ERR, "null context received");
			ret = -1;
			goto err;
		}

		*event = cq_entry.flags & event_mask;
		*lanep = cq_entry.op_context;

		return 0;
	}

	return 0;
err_cq_read:
	sret = fi_cq_readerr(cq, &err, 0);
	if (sret < 0) {
		RPMEMD_FI_ERR((int)sret, "error reading from completion queue: "
			"cannot read error from completion queue");
		goto err;
	}

	str_err = fi_cq_strerror(cq, err.prov_errno, NULL, NULL, 0);
	RPMEMD_LOG(ERR, "error reading from completion queue: %s", str_err);
err:
	return ret;
}

/*
 * rpmemd_fip_thread -- thread callback which processes persist
 * operation
 */
static void *
rpmemd_fip_thread(void *arg)
{
	struct rpmemd_fip_thread *thread = arg;
	struct rpmemd_fip *fip = thread->fip;
	struct rpmemd_fip_lane *lanep = NULL;
	uint64_t event = 0;
	int ret = 0;

	while (!fip->closing) {
		ret = rpmemd_fip_cq_read(fip, thread->cq, &lanep, &event,
			FI_SEND|FI_RECV);
		if (ret)
			goto err;

		if (unlikely(fip->closing))
			break;

		RPMEMD_ASSERT(lanep != NULL);
		if (event & FI_RECV)
			ret = rpmemd_fip_process_recv(fip, lanep);
		else if (event & FI_SEND)
			ret = rpmemd_fip_process_send(fip, lanep);
		if (ret)
			goto err;
	}

	return 0;
err:
	return (void *)(uintptr_t)ret;
}

/*
 * rpmemd_fip_get_def_nthreads -- get default number of threads for given
 * persistency method
 */
static size_t
rpmemd_fip_get_def_nthreads(struct rpmemd_fip *fip)
{
	RPMEMD_ASSERT(fip->nlanes > 0);
	switch (fip->persist_method) {
	case RPMEM_PM_APM:
	case RPMEM_PM_GPSPM:
		return fip->nlanes;
	default:
		RPMEMD_ASSERT(0);
		return 0;
	}
}

/*
 * rpmemd_fip_set_attr -- save required attributes in rpmemd_fip handle
 */
static void
rpmemd_fip_set_attr(struct rpmemd_fip *fip, struct rpmemd_fip_attr *attr)
{
	fip->addr = attr->addr;
	fip->size = attr->size;
	fip->persist_method = attr->persist_method;
	fip->persist = attr->persist;
	fip->memcpy_persist = attr->memcpy_persist;
	fip->deep_persist = attr->deep_persist;
	fip->ctx = attr->ctx;
	fip->buff_size = attr->buff_size;
	fip->pmsg_size = roundup(sizeof(struct rpmem_msg_persist) +
			fip->buff_size, (size_t)64);

	size_t max_nlanes = rpmem_fip_max_nlanes(fip->fi);
	RPMEMD_ASSERT(max_nlanes < UINT_MAX);
	fip->nlanes = min((unsigned)max_nlanes, attr->nlanes);

	if (attr->nthreads) {
		fip->nthreads = attr->nthreads;
	} else {
		/* use default */
		fip->nthreads = rpmemd_fip_get_def_nthreads(fip);
	}

	RPMEMD_ASSERT(fip->persist_method < MAX_RPMEM_PM);

	fip->lanes_per_thread = (fip->nlanes - 1) / fip->nthreads + 1;
	size_t cq_size_per_lane = rpmem_fip_cq_size(fip->persist_method,
			RPMEM_FIP_NODE_SERVER);

	fip->cq_size = fip->lanes_per_thread * cq_size_per_lane;
}

/*
 * rpmemd_fip_init_thread -- init worker thread
 */
static int
rpmemd_fip_init_thread(struct rpmemd_fip *fip, struct rpmemd_fip_thread *thread)
{
	thread->fip = fip;
	thread->lanes = malloc(fip->lanes_per_thread * sizeof(*thread->lanes));
	if (!thread->lanes) {
		RPMEMD_LOG(ERR, "!allocating thread lanes");
		goto err_alloc_lanes;
	}

	struct fi_cq_attr cq_attr = {
		.size = fip->cq_size,
		.flags = 0,
		.format = FI_CQ_FORMAT_MSG, /* need context and flags */
		.wait_obj = FI_WAIT_UNSPEC,
		.signaling_vector = 0,
		.wait_cond = FI_CQ_COND_NONE,
		.wait_set = NULL,
	};

	int ret = fi_cq_open(fip->domain, &cq_attr, &thread->cq, NULL);
	if (ret) {
		RPMEMD_FI_ERR(ret, "opening completion queue");
		goto err_cq_open;
	}

	return 0;
err_cq_open:
	free(thread->lanes);
err_alloc_lanes:
	return -1;
}

/*
 * rpmemd_fip_fini_thread -- deinitialize worker thread
 */
static void
rpmemd_fip_fini_thread(struct rpmemd_fip *fip, struct rpmemd_fip_thread *thread)
{
	RPMEMD_FI_CLOSE(thread->cq, "closing completion queue");
	free(thread->lanes);
}

/*
 * rpmemd_fip_init_threads -- initialize worker threads
 */
static int
rpmemd_fip_init_threads(struct rpmemd_fip *fip)
{
	RPMEMD_ASSERT(fip->lanes != NULL);
	RPMEMD_ASSERT(fip->nthreads > 0);

	fip->threads = calloc(fip->nthreads, sizeof(*fip->threads));
	if (!fip->threads) {
		RPMEMD_LOG(ERR, "!allocating threads");
		goto err_alloc_threads;
	}

	int ret;
	size_t i;
	for (i = 0; i < fip->nthreads; i++) {
		ret = rpmemd_fip_init_thread(fip, &fip->threads[i]);
		if (ret) {
			RPMEMD_LOG(ERR, "!initializing thread %zu", i);
			goto err_init_thread;
		}
	}

	for (size_t i = 0; i < fip->nlanes; i++) {
		size_t w = i % fip->nthreads;
		struct rpmemd_fip_thread *thread = &fip->threads[w];
		fip->lanes[i].base.cq = thread->cq;
		thread->lanes[thread->nlanes++] = &fip->lanes[i];
	}

	return 0;
err_init_thread:
	for (size_t j = 0; j < i; j++)
		rpmemd_fip_fini_thread(fip, &fip->threads[j]);
	free(fip->threads);
err_alloc_threads:
	return -1;
}

/*
 * rpmemd_fip_fini_threads -- deinitialize worker threads
 */
static void
rpmemd_fip_fini_threads(struct rpmemd_fip *fip)
{
	for (size_t i = 0; i < fip->nthreads; i++)
		rpmemd_fip_fini_thread(fip, &fip->threads[i]);
	free(fip->threads);
}

/*
 * rpmemd_fip_init -- initialize fabric provider
 */
struct rpmemd_fip *
rpmemd_fip_init(const char *node, const char *service,
	struct rpmemd_fip_attr *attr, struct rpmem_resp_attr *resp,
	enum rpmem_err *err)
{
	int ret;

	RPMEMD_ASSERT(resp);
	RPMEMD_ASSERT(err);
	RPMEMD_ASSERT(attr);
	RPMEMD_ASSERT(attr->persist);

	struct rpmemd_fip *fip = calloc(1, sizeof(*fip));
	if (!fip) {
		RPMEMD_LOG(ERR, "!allocating fabric handle");
		*err = RPMEM_ERR_FATAL;
		return NULL;
	}

	ret = rpmemd_fip_getinfo(fip, service, node, attr->provider);
	if (ret) {
		*err = RPMEM_ERR_BADPROVIDER;
		goto err_getinfo;
	}

	rpmemd_fip_set_attr(fip, attr);

	ret = rpmemd_fip_init_fabric_res(fip);
	if (ret) {
		*err = RPMEM_ERR_FATAL;
		goto err_init_fabric_res;
	}

	ret = rpmemd_fip_init_memory(fip);
	if (ret) {
		*err = RPMEM_ERR_FATAL;
		goto err_init_memory;
	}

	ret = rpmemd_fip_lanes_init(fip);
	if (ret) {
		*err = RPMEM_ERR_FATAL;
		goto err_init_lanes;
	}

	ret = rpmemd_fip_init_threads(fip);
	if (ret) {
		*err = RPMEM_ERR_FATAL;
		goto err_init_threads;
	}

	ret = rpmemd_fip_init_common(fip);
	if (ret) {
		*err = RPMEM_ERR_FATAL;
		goto err_init;
	}

	ret = fi_listen(fip->pep);
	if (ret) {
		*err = RPMEM_ERR_FATAL_CONN;
		goto err_fi_listen;
	}

	ret = rpmemd_fip_set_resp(fip, resp);
	if (ret) {
		*err = RPMEM_ERR_FATAL;
		goto err_set_resp;
	}

	return fip;
err_set_resp:
	RPMEMD_FI_CLOSE(fip->pep, "closing passive endpoint");
err_fi_listen:
	rpmemd_fip_fini_common(fip);
err_init:
	rpmemd_fip_fini_threads(fip);
err_init_threads:
	rpmemd_fip_fini_lanes(fip);
err_init_lanes:
	rpmemd_fip_fini_memory(fip);
err_init_memory:
	rpmemd_fip_fini_fabric_res(fip);
err_init_fabric_res:
	fi_freeinfo(fip->fi);
err_getinfo:
	free(fip);
	return NULL;
}

/*
 * rpmemd_fip_fini -- deinitialize fabric provider
 */
void
rpmemd_fip_fini(struct rpmemd_fip *fip)
{
	rpmemd_fip_fini_common(fip);
	rpmemd_fip_fini_threads(fip);
	rpmemd_fip_fini_lanes(fip);
	rpmemd_fip_fini_memory(fip);
	rpmemd_fip_fini_fabric_res(fip);
	fi_freeinfo(fip->fi);
	free(fip);
}

/*
 * rpmemd_fip_accept_one -- accept a single connection
 */
static int
rpmemd_fip_accept_one(struct rpmemd_fip *fip,
	struct fi_info *info, struct rpmemd_fip_lane *lanep)
{
	int ret;

	ret = rpmemd_fip_init_ep(fip, info, &lanep->base);
	if (ret)
		goto err_init_ep;

	ret = rpmemd_fip_post_common(fip, lanep);
	if (ret)
		goto err_post;

	ret = fi_accept(lanep->base.ep, NULL, 0);
	if (ret) {
		RPMEMD_FI_ERR(ret, "accepting connection request");
		goto err_accept;
	}

	fi_freeinfo(info);

	return 0;
err_accept:
err_post:
	rpmemd_fip_fini_ep(&lanep->base);
err_init_ep:
	fi_freeinfo(info);
	return -1;
}

/*
 * rpmemd_fip_accept -- accept a single connection request
 */
int
rpmemd_fip_accept(struct rpmemd_fip *fip, int timeout)
{
	int ret;
	struct fi_eq_cm_entry entry;
	uint32_t event;
	unsigned nreq = 0; /* number of connection requests */
	unsigned ncon = 0; /* number of connected endpoints */
	int connecting = 1;

	while (connecting && (nreq < fip->nlanes || ncon < fip->nlanes)) {
		ret = rpmem_fip_read_eq(fip->eq, &entry,
				&event, timeout);
		if (ret)
			goto err_read_eq;

		switch (event) {
		case FI_CONNREQ:
			ret = rpmemd_fip_accept_one(fip, entry.info,
					&fip->lanes[nreq]);
			if (ret)
				goto err_accept_one;
			nreq++;
			break;
		case FI_CONNECTED:
			ncon++;
			break;
		case FI_SHUTDOWN:
			connecting = 0;
			break;
		default:
			RPMEMD_ERR("unexpected event received (%u)", event);
			goto err_read_eq;

		}
	}

	return 0;
err_accept_one:
err_read_eq:
	return -1;
}

/*
 * rpmemd_fip_wait_close -- wait specified time for connection closed event
 */
int
rpmemd_fip_wait_close(struct rpmemd_fip *fip, int timeout)
{
	struct fi_eq_cm_entry entry;
	int lret = 0;
	uint32_t event;
	int ret;

	for (unsigned i = 0; i < fip->nlanes; i++) {
		ret = rpmem_fip_read_eq(fip->eq, &entry, &event, timeout);
		if (ret)
			lret = ret;
		if (event != FI_SHUTDOWN) {
			RPMEMD_ERR("unexpected event received "
				"(is %u expected %u)",
				event, FI_SHUTDOWN);
			errno = EINVAL;
			lret = -1;
		}
	}

	return lret;
}

/*
 * rpmemd_fip_close -- close the connection
 */
int
rpmemd_fip_close(struct rpmemd_fip *fip)
{
	int lret = 0;
	int ret;

	for (unsigned i = 0; i < fip->nlanes; i++) {
		ret = rpmemd_fip_fini_ep(&fip->lanes[i].base);
		if (ret)
			lret = ret;
	}

	return lret;
}

/*
 * rpmemd_fip_process_start -- start processing
 */
int
rpmemd_fip_process_start(struct rpmemd_fip *fip)
{
	unsigned i;
	for (i = 0; i < fip->nthreads; i++) {
		errno = os_thread_create(&fip->threads[i].thread, NULL,
				rpmemd_fip_thread, &fip->threads[i]);
		if (errno) {
			RPMEMD_ERR("!running thread thread");
			goto err_thread_create;
		}
	}

	return 0;
err_thread_create:
	return -1;
}

/*
 * rpmemd_fip_process_stop -- stop processing
 */
int
rpmemd_fip_process_stop(struct rpmemd_fip *fip)
{
	/* this stops all threads */
	util_fetch_and_or32(&fip->closing, 1);
	int ret;
	int lret = 0;

	for (size_t i = 0; i < fip->nthreads; i++) {
		struct rpmemd_fip_thread *thread = &fip->threads[i];
		ret = fi_cq_signal(thread->cq);
		if (ret) {
			RPMEMD_FI_ERR(ret, "sending signal to CQ");
			lret = ret;
		}
		void *tret;
		errno = os_thread_join(&thread->thread, &tret);
		if (errno) {
			RPMEMD_LOG(ERR, "!joining cq thread");
			lret = -1;
		} else {
			ret = (int)(uintptr_t)tret;
			if (ret) {
				RPMEMD_LOG(ERR,
					"cq thread failed with code -- %d",
					ret);
				lret = ret;
			}
		}
	}

	return lret;
}
