/*
 * Copyright 2018, Intel Corporation
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
 * util_pool_hdr_layout.c -- unit test for pool_hdr layout
 *
 * This test should be modified after every layout change. It's here to prevent
 * any accidental layout changes.
 */
#include "util.h"
#include "unittest.h"

#include "pool_hdr.h"

#define POOL_HDR_SIG_LEN_V1 (8)
#define POOL_HDR_UNUSED_LEN_V1 (1904)
#define POOL_HDR_UNUSED2_LEN_V1 (1976)
#define POOL_HDR_2K_CHECKPOINT (2048UL)

#define ARCH_FLAGS_SIZE_V1 (16)
#define ARCH_FLAGS_RESERVED_LEN_V1 (4)

#define SHUTDOWN_STATE_SIZE_V1 (64)
#define SHUTDOWN_STATE_RESERVED_LEN_V1 (39)

int
main(int argc, char *argv[])
{
	START(argc, argv, "util_pool_hdr_layout");

	ASSERT_ALIGNED_BEGIN(struct pool_hdr);
	ASSERT_ALIGNED_FIELD(struct pool_hdr, signature);
	ASSERT_FIELD_SIZE(signature, POOL_HDR_SIG_LEN_V1);
	ASSERT_ALIGNED_FIELD(struct pool_hdr, major);
	ASSERT_ALIGNED_FIELD(struct pool_hdr, compat_features);
	ASSERT_ALIGNED_FIELD(struct pool_hdr, incompat_features);
	ASSERT_ALIGNED_FIELD(struct pool_hdr, ro_compat_features);
	ASSERT_ALIGNED_FIELD(struct pool_hdr, poolset_uuid);
	ASSERT_ALIGNED_FIELD(struct pool_hdr, uuid);
	ASSERT_ALIGNED_FIELD(struct pool_hdr, prev_part_uuid);
	ASSERT_ALIGNED_FIELD(struct pool_hdr, next_part_uuid);
	ASSERT_ALIGNED_FIELD(struct pool_hdr, prev_repl_uuid);
	ASSERT_ALIGNED_FIELD(struct pool_hdr, next_repl_uuid);
	ASSERT_ALIGNED_FIELD(struct pool_hdr, crtime);
	ASSERT_ALIGNED_FIELD(struct pool_hdr, arch_flags);
	ASSERT_ALIGNED_FIELD(struct pool_hdr, unused);
	ASSERT_FIELD_SIZE(unused, POOL_HDR_UNUSED_LEN_V1);
	ASSERT_OFFSET_CHECKPOINT(struct pool_hdr, POOL_HDR_2K_CHECKPOINT);
	ASSERT_ALIGNED_FIELD(struct pool_hdr, unused2);
	ASSERT_FIELD_SIZE(unused2, POOL_HDR_UNUSED2_LEN_V1);
	ASSERT_ALIGNED_FIELD(struct pool_hdr, sds);
	ASSERT_ALIGNED_FIELD(struct pool_hdr, checksum);
	ASSERT_ALIGNED_CHECK(struct pool_hdr);

	ASSERT_ALIGNED_BEGIN(struct arch_flags);
	ASSERT_ALIGNED_FIELD(struct arch_flags, alignment_desc);
	ASSERT_ALIGNED_FIELD(struct arch_flags, machine_class);
	ASSERT_ALIGNED_FIELD(struct arch_flags, data);
	ASSERT_ALIGNED_FIELD(struct arch_flags, reserved);
	ASSERT_FIELD_SIZE(reserved, ARCH_FLAGS_RESERVED_LEN_V1);
	ASSERT_ALIGNED_FIELD(struct arch_flags, machine);
	ASSERT_ALIGNED_CHECK(struct arch_flags);
	UT_COMPILE_ERROR_ON(sizeof(struct arch_flags) != ARCH_FLAGS_SIZE_V1);

	ASSERT_ALIGNED_BEGIN(struct shutdown_state);
	ASSERT_ALIGNED_FIELD(struct shutdown_state, usc);
	ASSERT_ALIGNED_FIELD(struct shutdown_state, uuid);
	ASSERT_ALIGNED_FIELD(struct shutdown_state, dirty);
	ASSERT_ALIGNED_FIELD(struct shutdown_state, reserved);
	ASSERT_FIELD_SIZE(reserved, SHUTDOWN_STATE_RESERVED_LEN_V1);
	ASSERT_ALIGNED_FIELD(struct shutdown_state, checksum);
	ASSERT_ALIGNED_CHECK(struct shutdown_state);
	UT_COMPILE_ERROR_ON(sizeof(struct shutdown_state) !=
			SHUTDOWN_STATE_SIZE_V1);

	DONE(NULL);
}
