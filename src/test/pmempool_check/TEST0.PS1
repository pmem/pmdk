# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2018, Intel Corporation
#
#
# pmempool_check/TEST0 -- test for checking pools
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$POOL="$DIR\file.pool"
$LOG="out$Env:UNITTEST_NUM.log"

echo "PMEMLOG: consistent" > $LOG

expect_normal_exit $PMEMPOOL create log $POOL
check_file $POOL
expect_normal_exit $PMEMWRITE $POOL TEST
expect_normal_exit $PMEMPOOL check $POOL >> $LOG

echo "PMEMBLK: consistent" >> $LOG
rm $POOL -Force
expect_normal_exit $PMEMPOOL create blk 512 $POOL
check_file $POOL
expect_normal_exit $PMEMWRITE $POOL 0:w:TEST0
expect_normal_exit $PMEMWRITE $POOL 1:w:TEST0
expect_normal_exit $PMEMWRITE $POOL 2:w:TEST0
expect_normal_exit $PMEMPOOL check $POOL >> $LOG

echo "PMEMLOG: pool_hdr" >> $LOG
rm $POOL -Force
expect_normal_exit $PMEMPOOL create log $POOL
check_file $POOL
Invoke-Expression "$PMEMSPOIL -v $POOL pool_hdr.signature=ERROR >> $LOG"
expect_abnormal_exit $PMEMPOOL check $POOL >> $LOG

echo "PMEMLOG: pmemlog" >> $LOG
rm $POOL -Force
expect_normal_exit $PMEMPOOL create log $POOL
check_file $POOL
Invoke-Expression "$PMEMSPOIL -v $POOL pmemlog.start_offset=0 pmemlog.end_offset=0 >> $LOG"
expect_abnormal_exit $PMEMPOOL check $POOL >> $LOG

echo "PMEMBLK: pool_hdr" >> $LOG
rm $POOL -Force
expect_normal_exit $PMEMPOOL create blk 512 $POOL
check_file $POOL
Invoke-Expression "$PMEMSPOIL -v $POOL pool_hdr.signature=ERROR >> $LOG"
expect_abnormal_exit $PMEMPOOL check $POOL >> $LOG

check

pass
