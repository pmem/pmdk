# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2018, Intel Corporation
#
#
# pmempool_check/TEST10 -- test for checking pools with multiple replicas
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$POOLSET="$DIR\pool.set"
$POOL_R1_PART1="$DIR\pool.replica1.part1"
$POOL_R1_PART2="$DIR\pool.replica1.part2"
$POOL_R2_PART1="$DIR\pool.replica2.part1"
$POOL_R2_PART2="$DIR\pool.replica2.part2"

$LOG="out$Env:UNITTEST_NUM.log"
rm $LOG -Force -ea si

create_poolset $POOLSET `
	20M:$POOL_R1_PART1 50M:$POOL_R1_PART2 `
	R `
	40M:$POOL_R2_PART1 20M:$POOL_R2_PART2

expect_normal_exit $PMEMPOOL create obj $POOLSET
check_files $POOL_R1_PART1 $POOL_R1_PART2 $POOL_R2_PART1 $POOL_R2_PART2
&$PMEMSPOIL -v $POOL_R2_PART1 pool_hdr.uuid=ERROR >> $LOG
expect_abnormal_exit $PMEMPOOL check -vry $POOLSET >> $LOG
expect_abnormal_exit $PMEMPOOL info $POOLSET 2> $null

check

pass
