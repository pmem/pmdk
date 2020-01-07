# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2018, Intel Corporation
#
#
# pmempool_check/TEST8 -- test for checking pools
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$POOLSET="$DIR\pool.set"
$POOL_P1="$DIR\pool.p1"
$POOL_P2="$DIR\pool.p2"
$POOL_P3="$DIR\pool.p3"
$POOL_P4="$DIR\pool.p4"

$LOG="out$Env:UNITTEST_NUM.log"
rm $LOG -Force -ea si

create_poolset $POOLSET `
	32M:$POOL_P1 `
	32M:$POOL_P2 `
	32M:$POOL_P3 `
	32M:$POOL_P4

check_file $POOLSET

rm $POOL_P1, $POOL_P2, $POOL_P3, $POOL_P4 -Force -ea si

expect_normal_exit $PMEMPOOL create blk 512 $POOLSET
expect_normal_exit $PMEMPOOL info $POOLSET > $null
check_files $POOL_P1 $POOL_P2 $POOL_P3 $POOL_P4
&$PMEMSPOIL -v $POOL_P1 `
	pool_hdr.next_part_uuid=ERROR_NEXT `
	pool_hdr.prev_part_uuid=ERROR_PREV `
	pool_hdr.next_repl_uuid=ERROR_NEXT_REP `
	pool_hdr.prev_repl_uuid=ERROR_PREV_REP `
	pool_hdr.poolset_uuid=ERROR `
	>> $LOG

expect_abnormal_exit $PMEMPOOL check $POOLSET >> $LOG
expect_normal_exit $PMEMPOOL check -vyr $POOLSET >> $LOG
expect_normal_exit $PMEMPOOL check $POOLSET >> $LOG
expect_normal_exit $PMEMPOOL info $POOLSET > $null

check

pass
