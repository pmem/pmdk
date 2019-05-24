/*
 * Copyright 2016-2019, Intel Corporation
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

#ifndef RPMEM_PROTO_H
#define RPMEM_PROTO_H 1

#include <stdint.h>
#include <endian.h>

#include "librpmem.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PACKED	__attribute__((packed))

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
	RPMEM_MSG_TYPE_SET_ATTR		= 7, /* set attributes request */
	/* set attributes request response */
	RPMEM_MSG_TYPE_SET_ATTR_RESP	= 8,
	MAX_RPMEM_MSG_TYPE,
};

/*
 * rpmem_pool_attr_packed -- a packed version
 */
struct rpmem_pool_attr_packed {
	char signature[RPMEM_POOL_HDR_SIG_LEN]; /* pool signature */
	uint32_t major; /* format major version number */
	uint32_t compat_features; /* mask: compatible "may" features */
	uint32_t incompat_features; /* mask: "must support" features */
	uint32_t ro_compat_features; /* mask: force RO if unsupported */
	unsigned char poolset_uuid[RPMEM_POOL_HDR_UUID_LEN]; /* pool uuid */
	unsigned char uuid[RPMEM_POOL_HDR_UUID_LEN]; /* first part uuid */
	unsigned char next_uuid[RPMEM_POOL_HDR_UUID_LEN]; /* next pool uuid */
	unsigned char prev_uuid[RPMEM_POOL_HDR_UUID_LEN]; /* prev pool uuid */
	unsigned char user_flags[RPMEM_POOL_USER_FLAGS_LEN]; /* user flags */
} PACKED;

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
	uint8_t desc[0];	/* pool descriptor, null-terminated string */
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
 * rpmem_msg_common -- common fields for open/create messages
 */
struct rpmem_msg_common {
	uint16_t major;			/* protocol version major number */
	uint16_t minor;			/* protocol version minor number */
	uint64_t pool_size;		/* minimum required size of a pool */
	uint32_t nlanes;		/* number of lanes used by initiator */
	uint32_t provider;		/* provider */
	uint64_t buff_size;		/* buffer size for inline persist */
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
	struct rpmem_msg_common c;
	struct rpmem_pool_attr_packed pool_attr;	/* pool attributes */
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
	struct rpmem_msg_common c;
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
	struct rpmem_pool_attr_packed pool_attr; /* pool attributes */
} PACKED;

/*
 * rpmem_msg_close -- close request message
 *
 * The type of message must be set to RPMEM_MSG_TYPE_CLOSE
 * The size of message must be set to sizeof(struct rpmem_msg_close)
 */
struct rpmem_msg_close {
	struct rpmem_msg_hdr hdr;	/* message header */
	uint32_t flags;				/* flags */
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

#define RPMEM_FLUSH_WRITE	0U	/* flush / persist using RDMA WRITE */
#define RPMEM_DEEP_PERSIST	1U	/* deep persist operation */
#define RPMEM_PERSIST_SEND	2U	/* persist using RDMA SEND */
#define RPMEM_COMPLETION	4U	/* schedule command with a completion */

/* the two least significant bits are reserved for mode of persist */
#define RPMEM_FLUSH_PERSIST_MASK	0x3U

#define RPMEM_PERSIST_MAX		2U /* maximum valid persist value */

/*
 * rpmem_msg_persist -- remote persist message
 */
struct rpmem_msg_persist {
	uint32_t flags; /* lane flags */
	uint32_t lane;	/* lane identifier */
	uint64_t addr;	/* remote memory address */
	uint64_t size;	/* remote memory size */
	uint8_t data[];
};

/*
 * rpmem_msg_persist_resp -- remote persist response message
 */
struct rpmem_msg_persist_resp {
	uint32_t flags;	/* lane flags */
	uint32_t lane;	/* lane identifier */
};

/*
 * rpmem_msg_set_attr -- set attributes request message
 *
 * The type of message must be set to RPMEM_MSG_TYPE_SET_ATTR.
 * The size of message must be set to sizeof(struct rpmem_msg_set_attr)
 */
struct rpmem_msg_set_attr {
	struct rpmem_msg_hdr hdr;	/* message header */
	struct rpmem_pool_attr_packed pool_attr;	/* pool attributes */
} PACKED;

/*
 * rpmem_msg_set_attr_resp -- set attributes request response message
 *
 * The type of message must be set to RPMEM_MSG_TYPE_SET_ATTR_RESP.
 * The size of message must be set to sizeof(struct rpmem_msg_set_attr_resp).
 */
struct rpmem_msg_set_attr_resp {
	struct rpmem_msg_hdr_resp hdr;	/* message header */
} PACKED;

/*
 * XXX Begin: Suppress gcc conversion warnings for FreeBSD be*toh macros.
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
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
rpmem_ntoh_pool_attr(struct rpmem_pool_attr_packed *attr)
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
 * rpmem_ntoh_msg_common -- convert rpmem_msg_common to host byte order
 */
static inline void
rpmem_ntoh_msg_common(struct rpmem_msg_common *msg)
{
	msg->major = be16toh(msg->major);
	msg->minor = be16toh(msg->minor);
	msg->pool_size = be64toh(msg->pool_size);
	msg->nlanes = be32toh(msg->nlanes);
	msg->provider = be32toh(msg->provider);
	msg->buff_size = be64toh(msg->buff_size);
}

/*
 * rpmem_hton_msg_common -- convert rpmem_msg_common to network byte order
 */
static inline void
rpmem_hton_msg_common(struct rpmem_msg_common *msg)
{
	rpmem_ntoh_msg_common(msg);
}

/*
 * rpmem_ntoh_msg_create -- convert rpmem_msg_create to host byte order
 */
static inline void
rpmem_ntoh_msg_create(struct rpmem_msg_create *msg)
{
	rpmem_ntoh_msg_hdr(&msg->hdr);
	rpmem_ntoh_msg_common(&msg->c);
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
	rpmem_ntoh_msg_common(&msg->c);
	rpmem_ntoh_msg_pool_desc(&msg->pool_desc);
}
/*
 * XXX End: Suppress gcc conversion warnings for FreeBSD be*toh macros
 */
#pragma GCC diagnostic pop
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
 * rpmem_ntoh_msg_set_attr -- convert rpmem_msg_set_attr to host byte order
 */
static inline void
rpmem_ntoh_msg_set_attr(struct rpmem_msg_set_attr *msg)
{
	rpmem_ntoh_msg_hdr(&msg->hdr);
	rpmem_ntoh_pool_attr(&msg->pool_attr);
}

/*
 * rpmem_hton_msg_set_attr -- convert rpmem_msg_set_attr to network byte order
 */
static inline void
rpmem_hton_msg_set_attr(struct rpmem_msg_set_attr *msg)
{
	rpmem_ntoh_msg_set_attr(msg);
}

/*
 * rpmem_ntoh_msg_set_attr_resp -- convert rpmem_msg_set_attr_resp to host byte
 * order
 */
static inline void
rpmem_ntoh_msg_set_attr_resp(struct rpmem_msg_set_attr_resp *msg)
{
	rpmem_ntoh_msg_hdr_resp(&msg->hdr);
}

/*
 * rpmem_hton_msg_set_attr_resp -- convert rpmem_msg_set_attr_resp to network
 *	byte order
 */
static inline void
rpmem_hton_msg_set_attr_resp(struct rpmem_msg_set_attr_resp *msg)
{
	rpmem_hton_msg_hdr_resp(&msg->hdr);
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

/*
 * pack_rpmem_pool_attr -- copy pool attributes to a packed structure
 */
static inline void
pack_rpmem_pool_attr(const struct rpmem_pool_attr *src,
		struct rpmem_pool_attr_packed *dst)
{
	memcpy(dst->signature, src->signature, sizeof(src->signature));
	dst->major = src->major;
	dst->compat_features = src->compat_features;
	dst->incompat_features = src->incompat_features;
	dst->ro_compat_features = src->ro_compat_features;
	memcpy(dst->poolset_uuid, src->poolset_uuid, sizeof(dst->poolset_uuid));
	memcpy(dst->uuid, src->uuid, sizeof(dst->uuid));
	memcpy(dst->next_uuid, src->next_uuid, sizeof(dst->next_uuid));
	memcpy(dst->prev_uuid, src->prev_uuid, sizeof(dst->prev_uuid));
	memcpy(dst->user_flags, src->user_flags, sizeof(dst->user_flags));
}

/*
 * unpack_rpmem_pool_attr -- copy pool attributes to an unpacked structure
 */
static inline void
unpack_rpmem_pool_attr(const struct rpmem_pool_attr_packed *src,
		struct rpmem_pool_attr *dst)
{
	memcpy(dst->signature, src->signature, sizeof(src->signature));
	dst->major = src->major;
	dst->compat_features = src->compat_features;
	dst->incompat_features = src->incompat_features;
	dst->ro_compat_features = src->ro_compat_features;
	memcpy(dst->poolset_uuid, src->poolset_uuid, sizeof(dst->poolset_uuid));
	memcpy(dst->uuid, src->uuid, sizeof(dst->uuid));
	memcpy(dst->next_uuid, src->next_uuid, sizeof(dst->next_uuid));
	memcpy(dst->prev_uuid, src->prev_uuid, sizeof(dst->prev_uuid));
	memcpy(dst->user_flags, src->user_flags, sizeof(dst->user_flags));
}

#ifdef __cplusplus
}
#endif

#endif
