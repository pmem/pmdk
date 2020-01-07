// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2018, Intel Corporation */

/*
 * rpmem_fip_msg.h -- simple wrappers for fi_rma(3) and fi_msg(3) functions
 */

#ifndef RPMEM_FIP_MSG_H
#define RPMEM_FIP_MSG_H 1

#include <rdma/fi_rma.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * rpmem_fip_rma -- helper struct for RMA operation
 */
struct rpmem_fip_rma {
	struct fi_msg_rma msg;	/* message structure */
	struct iovec msg_iov;	/* IO vector buffer */
	struct fi_rma_iov rma_iov; /* RMA IO vector buffer */
	void *desc;		/* local memory descriptor */
	uint64_t flags;		/* RMA operation flags */
};

/*
 * rpmem_fip_msg -- helper struct for MSG operation
 */
struct rpmem_fip_msg {
	struct fi_msg msg;	/* message structure */
	struct iovec iov;	/* IO vector buffer */
	void *desc;		/* local memory descriptor */
	uint64_t flags;		/* MSG operation flags */
};

/*
 * rpmem_fip_rma_init -- initialize RMA helper struct
 */
static inline void
rpmem_fip_rma_init(struct rpmem_fip_rma *rma, void *desc,
	fi_addr_t addr, uint64_t rkey, void *context, uint64_t flags)
{
	memset(rma, 0, sizeof(*rma));
	rma->desc = desc;
	rma->flags = flags;
	rma->rma_iov.key = rkey;
	rma->msg.context = context;
	rma->msg.addr = addr;
	rma->msg.desc = &rma->desc;
	rma->msg.rma_iov = &rma->rma_iov;
	rma->msg.rma_iov_count = 1;
	rma->msg.msg_iov = &rma->msg_iov;
	rma->msg.iov_count = 1;
}

/*
 * rpmem_fip_msg_init -- initialize MSG helper struct
 */
static inline void
rpmem_fip_msg_init(struct rpmem_fip_msg *msg, void *desc, fi_addr_t addr,
	void *context, void *buff, size_t len, uint64_t flags)
{
	memset(msg, 0, sizeof(*msg));
	msg->desc = desc;
	msg->flags = flags;
	msg->iov.iov_base = buff;
	msg->iov.iov_len = len;
	msg->msg.context = context;
	msg->msg.addr = addr;
	msg->msg.desc = &msg->desc;
	msg->msg.msg_iov = &msg->iov;
	msg->msg.iov_count = 1;
}

/*
 * rpmem_fip_writemsg -- wrapper for fi_writemsg
 */
static inline int
rpmem_fip_writemsg(struct fid_ep *ep, struct rpmem_fip_rma *rma,
	const void *buff, size_t len, uint64_t addr)
{
	rma->rma_iov.addr = addr;
	rma->rma_iov.len = len;
	rma->msg_iov.iov_base = (void *)buff;
	rma->msg_iov.iov_len = len;
	return (int)fi_writemsg(ep, &rma->msg, rma->flags);
}

/*
 * rpmem_fip_readmsg -- wrapper for fi_readmsg
 */
static inline int
rpmem_fip_readmsg(struct fid_ep *ep, struct rpmem_fip_rma *rma,
	void *buff, size_t len, uint64_t addr)
{
	rma->rma_iov.addr = addr;
	rma->rma_iov.len = len;
	rma->msg_iov.iov_base = buff;
	rma->msg_iov.iov_len = len;
	return (int)fi_readmsg(ep, &rma->msg, rma->flags);
}

/*
 * rpmem_fip_sendmsg -- wrapper for fi_sendmsg
 */
static inline int
rpmem_fip_sendmsg(struct fid_ep *ep, struct rpmem_fip_msg *msg, size_t len)
{
	msg->iov.iov_len = len;
	return (int)fi_sendmsg(ep, &msg->msg, msg->flags);
}

/*
 * rpmem_fip_recvmsg -- wrapper for fi_recvmsg
 */
static inline int
rpmem_fip_recvmsg(struct fid_ep *ep, struct rpmem_fip_msg *msg)
{
	return (int)fi_recvmsg(ep, &msg->msg, msg->flags);
}

/*
 * rpmem_fip_msg_get_pmsg -- returns message buffer as a persist message
 */
static inline struct rpmem_msg_persist *
rpmem_fip_msg_get_pmsg(struct rpmem_fip_msg *msg)
{
	return (struct rpmem_msg_persist *)msg->iov.iov_base;
}

/*
 * rpmem_fip_msg_get_pres -- returns message buffer as a persist response
 */
static inline struct rpmem_msg_persist_resp *
rpmem_fip_msg_get_pres(struct rpmem_fip_msg *msg)
{
	return (struct rpmem_msg_persist_resp *)msg->iov.iov_base;
}

#ifdef __cplusplus
}
#endif

#endif
