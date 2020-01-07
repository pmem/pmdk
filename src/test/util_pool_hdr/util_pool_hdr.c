// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2020, Intel Corporation */

/*
 * util_pool_hdr.c -- unit test for pool_hdr layout and default values
 *
 * This test should be modified after every layout change. It's here to prevent
 * any accidental layout changes.
 */
#include "util.h"
#include "unittest.h"
#include "set.h"

#include "pool_hdr.h"

#define POOL_HDR_SIG_LEN_V1 (8)
#define POOL_HDR_UNUSED_LEN_V1 (1904)
#define POOL_HDR_UNUSED2_LEN_V1 (1976)
#define POOL_HDR_2K_CHECKPOINT (2048UL)

#define FEATURES_T_SIZE_V1 (12)

#define ARCH_FLAGS_SIZE_V1 (16)
#define ARCH_FLAGS_RESERVED_LEN_V1 (4)

#define SHUTDOWN_STATE_SIZE_V1 (64)
#define SHUTDOWN_STATE_RESERVED_LEN_V1 (39)

/*
 * test_layout -- test pool_hdr layout
 */
static void
test_layout()
{
	ASSERT_ALIGNED_BEGIN(struct pool_hdr);
	ASSERT_ALIGNED_FIELD(struct pool_hdr, signature);
	ASSERT_FIELD_SIZE(signature, POOL_HDR_SIG_LEN_V1);
	ASSERT_ALIGNED_FIELD(struct pool_hdr, major);
	ASSERT_ALIGNED_FIELD(struct pool_hdr, features);
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
#if PMEM_PAGESIZE > 4096
	ASSERT_ALIGNED_FIELD(struct pool_hdr, align_pad);
#endif
	ASSERT_ALIGNED_CHECK(struct pool_hdr);

	ASSERT_ALIGNED_BEGIN(features_t);
	ASSERT_ALIGNED_FIELD(features_t, compat);
	ASSERT_ALIGNED_FIELD(features_t, incompat);
	ASSERT_ALIGNED_FIELD(features_t, ro_compat);
	ASSERT_ALIGNED_CHECK(features_t);
	UT_COMPILE_ERROR_ON(sizeof(features_t) != FEATURES_T_SIZE_V1);

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
}

/* incompat features - final values */
#define POOL_FEAT_SINGLEHDR_FINAL	0x0001U
#define POOL_FEAT_CKSUM_2K_FINAL	0x0002U
#define POOL_FEAT_SDS_FINAL		0x0004U

/* incompat features effective values */
#if defined(_WIN32) || NDCTL_ENABLED
#ifdef SDS_ENABLED
#define POOL_E_FEAT_SDS_FINAL		POOL_FEAT_SDS_FINAL
#else
#define POOL_E_FEAT_SDS_FINAL		0x0000U	/* empty */
#endif
#else
/*
 * shutdown state support on Linux requires root access on kernel < 4.20 with
 * ndctl < 63 so it is disabled by default
 */
#define POOL_E_FEAT_SDS_FINAL		0x0000U	/* empty */
#endif

#define POOL_FEAT_INCOMPAT_DEFAULT_V1 \
	(POOL_FEAT_CKSUM_2K_FINAL | POOL_E_FEAT_SDS_FINAL)

#ifdef _WIN32
#define SDS_AT_CREATE_EXPECTED 1
#else
#define SDS_AT_CREATE_EXPECTED 0
#endif

/*
 * test_default_values -- test default values
 */
static void
test_default_values()
{
	UT_COMPILE_ERROR_ON(POOL_FEAT_SINGLEHDR != POOL_FEAT_SINGLEHDR_FINAL);
	UT_COMPILE_ERROR_ON(POOL_FEAT_CKSUM_2K != POOL_FEAT_CKSUM_2K_FINAL);
	UT_COMPILE_ERROR_ON(POOL_FEAT_SDS != POOL_FEAT_SDS_FINAL);
	UT_COMPILE_ERROR_ON(SDS_at_create != SDS_AT_CREATE_EXPECTED);

	UT_COMPILE_ERROR_ON(POOL_FEAT_INCOMPAT_DEFAULT !=
			POOL_FEAT_INCOMPAT_DEFAULT_V1);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "util_pool_hdr");

	test_layout();
	test_default_values();

	DONE(NULL);
}
