# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2018, Intel Corporation
#
#
# pmempool_check/TEST7 -- test for checking pools
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$POOL="$DIR\file.pool"
$LOG="out$Env:UNITTEST_NUM.log"
rm $LOG -Force -ea si

expect_normal_exit $PMEMPOOL create blk 512 $POOL
check_file $POOL
&$PMEMSPOIL -v $POOL pool_hdr.uuid=ERROR >> $LOG
expect_abnormal_exit $PMEMPOOL check $POOL >> $LOG
expect_normal_exit $PMEMPOOL check -vyr $POOL >> $LOG
expect_normal_exit $PMEMPOOL check -v $POOL >> $LOG

rm $POOL -Force -ea si
expect_normal_exit $PMEMPOOL create blk 512 $POOL
check_file $POOL
&$PMEMSPOIL -v $POOL pool_hdr.prev_part_uuid=ERROR_PREV pool_hdr.next_part_uuid=ERROR_NEXT >> $LOG
expect_abnormal_exit $PMEMPOOL check $POOL >> $LOG
expect_abnormal_exit $PMEMPOOL check -vyr $POOL >> $LOG
expect_normal_exit $PMEMPOOL check -avyr $POOL >> $LOG
expect_normal_exit $PMEMPOOL check -v $POOL >> $LOG

rm $POOL -Force -ea si
expect_normal_exit $PMEMPOOL create blk 512 $POOL
check_file $POOL
&$PMEMSPOIL -v $POOL pool_hdr.prev_part_uuid=ERROR_PREV pool_hdr.next_part_uuid=ERROR_NEXT pool_hdr.prev_repl_uuid=ERROR_RPREV pool_hdr.next_repl_uuid=ERROR_RNEXT >> $LOG
expect_abnormal_exit $PMEMPOOL check $POOL >> $LOG
expect_abnormal_exit $PMEMPOOL check -vyr $POOL >> $LOG
expect_normal_exit $PMEMPOOL check -avyr $POOL >> $LOG
expect_normal_exit $PMEMPOOL check -v $POOL >> $LOG

check

pass
