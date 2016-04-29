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
 * rpmem_proto.h -- rpmem protocol definitions
 */

#include <stdint.h>
#include <endian.h>

#include "librpmem.h"

#define __STR(s)	#s
#define _STR(s)		__STR(s)

#define PACKED	__attribute__((packed))

#define RPMEM_PORT		7636
#define RPMEM_SERVICE		_STR(RPMEM_PORT)
#define RPMEM_PROTO		"tcp"
#define RPMEM_PROTO_MAJOR	0
#define RPMEM_PROTO_MINOR	1
#define RPMEM_SIG_SIZE		8
#define RPMEM_UUID_SIZE		16
#define RPMEM_PROV_SIZE		32
#define RPMEM_USER_SIZE		16

/*
 * rpmem_msg_type -- type of messages
 */
enum rpmem_msg_type {
	RPMEM_MSG_TYPE_CREATE		= 1, /* create request */
	RPMEM_MSG_TYPE_CREATE_RESP	= 2, /* create request response */
	RPMEM_MSG_TYPE_OPEN		= 3, /* open request */
	RPMEM_MSG_TYPE_OPEN_RESP	= 4, /* open request response */
	RPMEM_MSG_TYPE_CLOSE		= 5, /* close request */
	RPMEM_MSG_TYPE_CLOSE_RESP	= 6, /* close request response */
	RPMEM_MSG_TYPE_REMOVE		= 7, /* remove request */
	RPMEM_MSG_TYPE_REMOVE_RESP	= 8, /* remove request response */

	MAX_RPMEM_MSG_TYPE,
};

/*
 * rpmem_msg_ibc_attr -- in-band connection attributes
 *
 * Used by create request response and open request response.
 * Contains essential information to proceed with in-band connection
 * initialization.
 */
struct rpmem_msg_ibc_attr {
	uint32_t port;			/* RDMA connection port */
	uint32_t persist_method;	/* persist method */
	uint64_t rkey;			/* remote key */
	uint64_t raddr;			/* remote address */
	uint32_t nlanes;		/* number of lanes */
} PACKED;

/*
 * rpmem_msg_pool_desc -- remote pool descriptor
 */
struct rpmem_msg_pool_desc {
	uint32_t size;		/* size of pool descriptor */
	uint8_t desc[0];	/* pool descriptor, NULL-terminated string */
} PACKED;

/*
 * rpmem_msg_hdr -- message header which consists of type and size of message
 *
 * The type must be one of the rpmem_msg_type values.
 */
struct rpmem_msg_hdr {
	uint32_t type;			/* type of message */
	uint64_t size;			/* size of message */
	uint8_t body[0];
} PACKED;

/*
 * rpmem_msg_hdr_resp -- message response header which consists of type, size
 * and status.
 *
 * The type must be one of the rpmem_msg_type values.
 */
struct rpmem_msg_hdr_resp {
	uint32_t status;		/* response status */
	uint32_t type;			/* type of message */
	uint64_t size;			/* size of message */
} PACKED;

/*
 * rpmem_msg_create -- create request message
 *
 * The type of message must be set to RPMEM_MSG_TYPE_CREATE.
 * The size of message must be set to
 *     sizeof(struct rpmem_msg_create) + pool_desc_size
 */
struct rpmem_msg_create {
	struct rpmem_msg_hdr hdr;	/* message header */
	uint16_t major;			/* protocol version major number */
	uint16_t minor;			/* protocol version minor number */
	uint64_t pool_size;		/* minimum required size of a pool */
	uint32_t nlanes;		/* number of lanes used by initiator */
	uint32_t provider;		/* provider */
	struct rpmem_pool_attr pool_attr;	/* pool attributes */
	struct rpmem_msg_pool_desc pool_desc;	/* pool descriptor */
} PACKED;

/*
 * rpmem_msg_create_resp -- create request response message
 *
 * The type of message must be set to RPMEM_MSG_TYPE_CREATE_RESP.
 * The size of message must be set to sizeof(struct rpmem_msg_create_resp).
 */
struct rpmem_msg_create_resp {
	struct rpmem_msg_hdr_resp hdr;	/* message header */
	struct rpmem_msg_ibc_attr ibc;	/* in-band connection attributes */
} PACKED;

/*
 * rpmem_msg_open -- open request message
 *
 * The type of message must be set to RPMEM_MSG_TYPE_OPEN.
 * The size of message must be set to
 *     sizeof(struct rpmem_msg_open) + pool_desc_size
 */
struct rpmem_msg_open {
	struct rpmem_msg_hdr hdr;	/* message header */
	uint16_t major;			/* protocol version major number */
	uint16_t minor;			/* protocol version minor number */
	uint64_t pool_size;		/* minimum required size of a pool */
	uint32_t nlanes;		/* number of lanes used by initiator */
	uint32_t provider;		/* provider */
	struct rpmem_msg_pool_desc pool_desc;	/* pool descriptor */
} PACKED;

/*
 * rpmem_msg_open_resp -- open request response message
 *
 * The type of message must be set to RPMEM_MSG_TYPE_OPEN_RESP.
 * The size of message must be set to sizeof(struct rpmem_msg_open_resp)
 */
struct rpmem_msg_open_resp {
	struct rpmem_msg_hdr_resp hdr;	/* message header */
	struct rpmem_msg_ibc_attr ibc;	/* in-band connection attributes */
	struct rpmem_pool_attr pool_attr; /* pool attributes */
} PACKED;

/*
 * rpmem_msg_close -- close request message
 *
 * The type of message must be set to RPMEM_MSG_TYPE_CLOSE
 * The size of message must be set to sizeof(struct rpmem_msg_close)
 */
struct rpmem_msg_close {
	struct rpmem_msg_hdr hdr;	/* message header */
	/* no more fields */
} PACKED;

/*
 * rpmem_msg_close_resp -- close request response message
 *
 * The type of message must be set to RPMEM_MSG_TYPE_CLOSE_RESP
 * The size of message must be set to sizeof(struct rpmem_msg_close_resp)
 */
struct rpmem_msg_close_resp {
	struct rpmem_msg_hdr_resp hdr;	/* message header */
	/* no more fields */
} PACKED;

/*
 * rpmem_msg_remove -- remove request message
 *
 * The type of message must be set to RPMEM_MSG_TYPE_REMOVE.
 * The size of message must be set to
 *     sizeof(struct rpmem_msg_remove) + pool_desc_size
 */
struct rpmem_msg_remove {
	struct rpmem_msg_hdr hdr;	/* message header */
	uint16_t major;			/* protocol version major number */
	uint16_t minor;			/* protocol version minor number */
	struct rpmem_msg_pool_desc pool_desc;	/* pool descriptor */
} PACKED;

/*
 * rpmem_msg_remove_resp -- remove request response message
 *
 * The type of message must be set to RPMEM_MSG_TYPE_REMOVE_RESP
 * The size of message must be set to sizeof(struct rpmem_msg_remove_resp)
 */
struct rpmem_msg_remove_resp {
	struct rpmem_msg_hdr_resp hdr;	/* message header */
	/* no more fields */
} PACKED;

/*
 * rpmem_msg_persist -- remote persist message
 */
struct rpmem_msg_persist {
	uint64_t lane;	/* lane identifier */
	uint64_t addr;	/* remote memory address */
	uint64_t size;	/* remote memory size */
};

/*
 * rpmem_msg_persist_resp -- remote persist response message
 */
struct rpmem_msg_persist_resp {
	uint64_t lane;	/* lane identifier */
};

/*
 * rpmem_ntoh_msg_ibc_attr -- convert rpmem_msg_ibc attr to host byte order
 */
static inline void
rpmem_ntoh_msg_ibc_attr(struct rpmem_msg_ibc_attr *ibc)
{
	ibc->port = be32toh(ibc->port);
	ibc->persist_method = be32toh(ibc->persist_method);
	ibc->rkey = be64toh(ibc->rkey);
	ibc->raddr = be64toh(ibc->raddr);
}

/*
 * rpmem_ntoh_msg_pool_desc -- convert rpmem_msg_pool_desc to host byte order
 */
static inline void
rpmem_ntoh_msg_pool_desc(struct rpmem_msg_pool_desc *pool_desc)
{
	pool_desc->size = be32toh(pool_desc->size);
}

/*
 * rpmem_ntoh_pool_attr -- convert rpmem_pool_attr to host byte order
 */
static inline void
rpmem_ntoh_pool_attr(struct rpmem_pool_attr *attr)
{
	attr->major = be32toh(attr->major);
	attr->ro_compat_features = be32toh(attr->ro_compat_features);
	attr->incompat_features = be32toh(attr->incompat_features);
	attr->compat_features = be32toh(attr->compat_features);
}

/*
 * rpmem_ntoh_msg_hdr -- convert rpmem_msg_hdr to host byte order
 */
static inline void
rpmem_ntoh_msg_hdr(struct rpmem_msg_hdr *hdrp)
{
	hdrp->type = be32toh(hdrp->type);
	hdrp->size = be64toh(hdrp->size);
}

/*
 * rpmem_hton_msg_hdr -- convert rpmem_msg_hdr to network byte order
 */
static inline void
rpmem_hton_msg_hdr(struct rpmem_msg_hdr *hdrp)
{
	rpmem_ntoh_msg_hdr(hdrp);
}

/*
 * rpmem_ntoh_msg_hdr_resp -- convert rpmem_msg_hdr_resp to host byte order
 */
static inline void
rpmem_ntoh_msg_hdr_resp(struct rpmem_msg_hdr_resp *hdrp)
{
	hdrp->status = be32toh(hdrp->status);
	hdrp->type = be32toh(hdrp->type);
	hdrp->size = be64toh(hdrp->size);
}

/*
 * rpmem_hton_msg_hdr_resp -- convert rpmem_msg_hdr_resp to network byte order
 */
static inline void
rpmem_hton_msg_hdr_resp(struct rpmem_msg_hdr_resp *hdrp)
{
	rpmem_ntoh_msg_hdr_resp(hdrp);
}

/*
 * rpmem_ntoh_msg_create -- convert rpmem_msg_create to host byte order
 */
static inline void
rpmem_ntoh_msg_create(struct rpmem_msg_create *msg)
{
	rpmem_ntoh_msg_hdr(&msg->hdr);
	msg->major = be16toh(msg->major);
	msg->minor = be16toh(msg->minor);
	msg->pool_size = be64toh(msg->pool_size);
	msg->nlanes = be32toh(msg->nlanes);
	msg->provider = be32toh(msg->provider);
	rpmem_ntoh_pool_attr(&msg->pool_attr);
	rpmem_ntoh_msg_pool_desc(&msg->pool_desc);
}

/*
 * rpmem_hton_msg_create -- convert rpmem_msg_create to network byte order
 */
static inline void
rpmem_hton_msg_create(struct rpmem_msg_create *msg)
{
	rpmem_ntoh_msg_create(msg);
}

/*
 * rpmem_ntoh_msg_create_resp -- convert rpmem_msg_create_resp to host byte
 * order
 */
static inline void
rpmem_ntoh_msg_create_resp(struct rpmem_msg_create_resp *msg)
{
	rpmem_ntoh_msg_hdr_resp(&msg->hdr);
	rpmem_ntoh_msg_ibc_attr(&msg->ibc);
}

/*
 * rpmem_hton_msg_create_resp -- convert rpmem_msg_create_resp to network byte
 * order
 */
static inline void
rpmem_hton_msg_create_resp(struct rpmem_msg_create_resp *msg)
{
	rpmem_ntoh_msg_create_resp(msg);
}

/*
 * rpmem_ntoh_msg_open -- convert rpmem_msg_open to host byte order
 */
static inline void
rpmem_ntoh_msg_open(struct rpmem_msg_open *msg)
{
	rpmem_ntoh_msg_hdr(&msg->hdr);
	msg->major = be16toh(msg->major);
	msg->minor = be16toh(msg->minor);
	msg->pool_size = be64toh(msg->pool_size);
	msg->nlanes = be32toh(msg->nlanes);
	msg->provider = be32toh(msg->provider);
	rpmem_ntoh_msg_pool_desc(&msg->pool_desc);
}

/*
 * rpmem_hton_msg_open -- convert rpmem_msg_open to network byte order
 */
static inline void
rpmem_hton_msg_open(struct rpmem_msg_open *msg)
{
	rpmem_ntoh_msg_open(msg);
}

/*
 * rpmem_ntoh_msg_open_resp -- convert rpmem_msg_open_resp to host byte order
 */
static inline void
rpmem_ntoh_msg_open_resp(struct rpmem_msg_open_resp *msg)
{
	rpmem_ntoh_msg_hdr_resp(&msg->hdr);
	rpmem_ntoh_msg_ibc_attr(&msg->ibc);
	rpmem_ntoh_pool_attr(&msg->pool_attr);
}

/*
 * rpmem_hton_msg_open_resp -- convert rpmem_msg_open_resp to network byte order
 */
static inline void
rpmem_hton_msg_open_resp(struct rpmem_msg_open_resp *msg)
{
	rpmem_ntoh_msg_open_resp(msg);
}

/*
 * rpmem_ntoh_msg_remove -- convert rpmem_msg_remove to host byte order
 */
static inline void
rpmem_ntoh_msg_remove(struct rpmem_msg_remove *msg)
{
	rpmem_ntoh_msg_hdr(&msg->hdr);
	msg->major = be16toh(msg->major);
	msg->minor = be16toh(msg->minor);
	rpmem_ntoh_msg_pool_desc(&msg->pool_desc);
}

/*
 * rpmem_hton_msg_remove -- convert rpmem_msg_remove to network byte order
 */
static inline void
rpmem_hton_msg_remove(struct rpmem_msg_remove *msg)
{
	rpmem_ntoh_msg_remove(msg);
}

/*
 * rpmem_ntoh_msg_remove_resp -- convert rpmem_msg_remove_resp to host byte
 * order
 */
static inline void
rpmem_ntoh_msg_remove_resp(struct rpmem_msg_remove_resp *msg)
{
	rpmem_ntoh_msg_hdr_resp(&msg->hdr);
}

/*
 * rpmem_hton_msg_remove_resp -- convert rpmem_msg_remove_resp to network byte
 * order
 */
static inline void
rpmem_hton_msg_remove_resp(struct rpmem_msg_remove_resp *msg)
{
	rpmem_ntoh_msg_remove_resp(msg);
}

/*
 * rpmem_ntoh_msg_close -- convert rpmem_msg_close to host byte order
 */
static inline void
rpmem_ntoh_msg_close(struct rpmem_msg_close *msg)
{
	rpmem_ntoh_msg_hdr(&msg->hdr);
}

/*
 * rpmem_hton_msg_close -- convert rpmem_msg_close to network byte order
 */
static inline void
rpmem_hton_msg_close(struct rpmem_msg_close *msg)
{
	rpmem_ntoh_msg_close(msg);
}

/*
 * rpmem_ntoh_msg_close_resp -- convert rpmem_msg_close_resp to host byte order
 */
static inline void
rpmem_ntoh_msg_close_resp(struct rpmem_msg_close_resp *msg)
{
	rpmem_ntoh_msg_hdr_resp(&msg->hdr);
}

/*
 * rpmem_hton_msg_close_resp -- convert rpmem_msg_close_resp to network byte
 * order
 */
static inline void
rpmem_hton_msg_close_resp(struct rpmem_msg_close_resp *msg)
{
	rpmem_ntoh_msg_close_resp(msg);
}
