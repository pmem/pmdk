/*
 * Copyright 2016-2018, Intel Corporation
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
 * rpmem_proto.c -- unit test for rpmem_proto header
 *
 * The purpose of this test is to make sure the structures which describe
 * rpmem protocol messages does not have any padding.
 */

#include "unittest.h"

#include "librpmem.h"
#include "rpmem_proto.h"

int
main(int argc, char *argv[])
{
	START(argc, argv, "rpmem_proto");

	ASSERT_ALIGNED_BEGIN(struct rpmem_msg_hdr);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_hdr, type);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_hdr, size);
	ASSERT_ALIGNED_CHECK(struct rpmem_msg_hdr);

	ASSERT_ALIGNED_BEGIN(struct rpmem_msg_hdr_resp);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_hdr_resp, status);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_hdr_resp, type);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_hdr_resp, size);
	ASSERT_ALIGNED_CHECK(struct rpmem_msg_hdr_resp);

	ASSERT_ALIGNED_BEGIN(struct rpmem_pool_attr);
	ASSERT_ALIGNED_FIELD(struct rpmem_pool_attr, signature);
	ASSERT_ALIGNED_FIELD(struct rpmem_pool_attr, major);
	ASSERT_ALIGNED_FIELD(struct rpmem_pool_attr, compat_features);
	ASSERT_ALIGNED_FIELD(struct rpmem_pool_attr, incompat_features);
	ASSERT_ALIGNED_FIELD(struct rpmem_pool_attr, ro_compat_features);
	ASSERT_ALIGNED_FIELD(struct rpmem_pool_attr, poolset_uuid);
	ASSERT_ALIGNED_FIELD(struct rpmem_pool_attr, uuid);
	ASSERT_ALIGNED_FIELD(struct rpmem_pool_attr, next_uuid);
	ASSERT_ALIGNED_FIELD(struct rpmem_pool_attr, prev_uuid);
	ASSERT_ALIGNED_FIELD(struct rpmem_pool_attr, user_flags);
	ASSERT_ALIGNED_CHECK(struct rpmem_pool_attr);

	ASSERT_ALIGNED_BEGIN(struct rpmem_pool_attr_packed);
	ASSERT_ALIGNED_FIELD(struct rpmem_pool_attr_packed, signature);
	ASSERT_ALIGNED_FIELD(struct rpmem_pool_attr_packed, major);
	ASSERT_ALIGNED_FIELD(struct rpmem_pool_attr_packed, compat_features);
	ASSERT_ALIGNED_FIELD(struct rpmem_pool_attr_packed, incompat_features);
	ASSERT_ALIGNED_FIELD(struct rpmem_pool_attr_packed, ro_compat_features);
	ASSERT_ALIGNED_FIELD(struct rpmem_pool_attr_packed, poolset_uuid);
	ASSERT_ALIGNED_FIELD(struct rpmem_pool_attr_packed, uuid);
	ASSERT_ALIGNED_FIELD(struct rpmem_pool_attr_packed, next_uuid);
	ASSERT_ALIGNED_FIELD(struct rpmem_pool_attr_packed, prev_uuid);
	ASSERT_ALIGNED_FIELD(struct rpmem_pool_attr_packed, user_flags);
	ASSERT_ALIGNED_CHECK(struct rpmem_pool_attr_packed);

	ASSERT_ALIGNED_BEGIN(struct rpmem_msg_ibc_attr);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_ibc_attr, port);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_ibc_attr, persist_method);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_ibc_attr, rkey);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_ibc_attr, raddr);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_ibc_attr, nlanes);
	ASSERT_ALIGNED_CHECK(struct rpmem_msg_ibc_attr);

	ASSERT_ALIGNED_BEGIN(struct rpmem_msg_common);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_common, major);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_common, minor);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_common, pool_size);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_common, nlanes);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_common, provider);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_common, buff_size);
	ASSERT_ALIGNED_CHECK(struct rpmem_msg_common);

	ASSERT_ALIGNED_BEGIN(struct rpmem_msg_pool_desc);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_pool_desc, size);
	ASSERT_ALIGNED_CHECK(struct rpmem_msg_pool_desc);

	ASSERT_ALIGNED_BEGIN(struct rpmem_msg_create);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_create, hdr);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_create, c);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_create, pool_attr);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_create, pool_desc);
	ASSERT_ALIGNED_CHECK(struct rpmem_msg_create);

	ASSERT_ALIGNED_BEGIN(struct rpmem_msg_create_resp);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_create_resp, hdr);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_create_resp, ibc);
	ASSERT_ALIGNED_CHECK(struct rpmem_msg_create_resp);

	ASSERT_ALIGNED_BEGIN(struct rpmem_msg_open);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_open, hdr);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_open, c);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_open, pool_desc);
	ASSERT_ALIGNED_CHECK(struct rpmem_msg_open);

	ASSERT_ALIGNED_BEGIN(struct rpmem_msg_open_resp);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_open_resp, hdr);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_open_resp, ibc);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_open_resp, pool_attr);
	ASSERT_ALIGNED_CHECK(struct rpmem_msg_open_resp);

	ASSERT_ALIGNED_BEGIN(struct rpmem_msg_close);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_close, hdr);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_close, flags);
	ASSERT_ALIGNED_CHECK(struct rpmem_msg_close);

	ASSERT_ALIGNED_BEGIN(struct rpmem_msg_close_resp);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_close_resp, hdr);
	ASSERT_ALIGNED_CHECK(struct rpmem_msg_close_resp);

	ASSERT_ALIGNED_BEGIN(struct rpmem_msg_persist);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_persist, flags);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_persist, lane);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_persist, addr);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_persist, size);
	ASSERT_ALIGNED_CHECK(struct rpmem_msg_persist);

	ASSERT_ALIGNED_BEGIN(struct rpmem_msg_persist_resp);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_persist_resp, flags);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_persist_resp, lane);
	ASSERT_ALIGNED_CHECK(struct rpmem_msg_persist_resp);

	ASSERT_ALIGNED_BEGIN(struct rpmem_msg_set_attr);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_set_attr, hdr);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_set_attr, pool_attr);
	ASSERT_ALIGNED_CHECK(struct rpmem_msg_set_attr);

	ASSERT_ALIGNED_BEGIN(struct rpmem_msg_set_attr_resp);
	ASSERT_ALIGNED_FIELD(struct rpmem_msg_set_attr_resp, hdr);
	ASSERT_ALIGNED_CHECK(struct rpmem_msg_set_attr_resp);

	DONE(NULL);
}
