# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019, Intel Corporation

#
# libpmempool_feature/TEST3 -- unit test for invalid features
#

. ..\unittest\unittest.ps1

require_test_type medium

require_build_type debug

require_fs_type any

setup
. .\common.ps1

# test single file pool with invalid compat features
expect_normal_exit $PMEMPOOL create obj $POOL
expect_normal_exit $PMEMSPOIL -v $POOL `
	pool_hdr.features.compat=0xfe `
	"pool_hdr.f:checksum_gen" >> $LOG

libpmempool_feature_query_abnormal "CKSUM_2K"

# test multipart poolset with invalid incompat features in one of its part
$POOLSET="$DIR\testset"
$POOL=$POOLSET
$TARGET_PART1="$DIR\testfile23"
$TARGET_PART2="$DIR\testfile11"
$PART_SIZE = (convert_to_bytes "10M")
$PART_SIZE_STR = ${PART_SIZE}.toString() + "B" #10MiB

create_poolset $POOLSET `
	${PART_SIZE_STR}:${TARGET_PART2}:x ${PART_SIZE_STR}:$DIR\testfile12:x `
	R ${PART_SIZE_STR}:$DIR\testfile21:x ${PART_SIZE_STR}:$DIR\testfile22:x `
	${PART_SIZE_STR}:${TARGET_PART1}:x

expect_normal_exit $PMEMPOOL create obj $POOLSET
expect_normal_exit $PMEMSPOIL -v $TARGET_PART1 `
	pool_hdr.features.incompat=0xfe `
	"pool_hdr.f:checksum_gen" >> $LOG

libpmempool_feature_query_abnormal "CKSUM_2K"

# test invalid rocompat features in first of its part
expect_normal_exit $PMEMSPOIL -v $TARGET_PART2 `
	pool_hdr.features.ro_compat=0xfe `
	"pool_hdr.f:checksum_gen" >> $LOG

libpmempool_feature_query_abnormal "CKSUM_2K"

check

pass
