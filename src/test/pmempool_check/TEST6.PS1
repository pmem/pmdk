# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2018, Intel Corporation
#
#
# pmempool_check/TEST6 -- test for checking pools
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$POOLSET="$DIR\pool.set"
$POOL_PART1="$DIR\pool.part1"
$POOL_PART2="$DIR\pool.part2"

$LOG="out$Env:UNITTEST_NUM.log"
rm $LOG -Force -ea si

create_poolset $POOLSET 32M:$DIR\pool.part1:z 32M:$DIR\pool.part2
check_file $POOLSET

echo "PMEMLOG: pool_hdr" >> $LOG
expect_normal_exit $PMEMPOOL create log $POOLSET
check_files $POOL_PART1 $POOL_PART2
&$PMEMSPOIL -v $POOLSET pool_hdr.signature=ERROR >> $LOG
expect_abnormal_exit $PMEMPOOL check $POOLSET >> $LOG
expect_abnormal_exit $PMEMPOOL check $POOL_PART1 >> $LOG
expect_normal_exit $PMEMPOOL check $POOL_PART2 >> $LOG

echo "PMEMLOG: pmemlog" >> $LOG
rm $POOL_PART1, $POOL_PART2 -Force -ea si
expect_normal_exit $PMEMPOOL create log $POOLSET
check_files $POOL_PART1 $POOL_PART2
&$PMEMSPOIL -v $POOLSET pmemlog.start_offset=0 pmemlog.end_offset=0 >> $LOG
expect_abnormal_exit $PMEMPOOL check $POOLSET >> $LOG
expect_normal_exit $PMEMPOOL check $POOL_PART1 >> $LOG
expect_normal_exit $PMEMPOOL check $POOL_PART2 >> $LOG

echo "PMEMBLK: pool_hdr" >> $LOG
rm $POOL_PART1, $POOL_PART2 -Force -ea si
expect_normal_exit $PMEMPOOL create blk 512 $POOLSET
check_files $POOL_PART1 $POOL_PART2
&$PMEMSPOIL -v $POOLSET pool_hdr.signature=ERROR >> $LOG
expect_abnormal_exit $PMEMPOOL check $POOLSET >> $LOG
expect_abnormal_exit $PMEMPOOL check $POOL_PART1 >> $LOG
expect_normal_exit $PMEMPOOL check $POOL_PART2 >> $LOG

check

pass
