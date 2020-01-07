# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2018, Intel Corporation
#
#
# pmempool_check/TEST4 -- test for checking pools
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$POOL="$DIR\file.pool"
$LOG="out$Env:UNITTEST_NUM.log"

rm $LOG -Force -ea si

expect_normal_exit $PMEMPOOL create -w blk 512 $POOL
check_file $POOL
expect_normal_exit $PMEMPOOL info $POOL > $null
&$PMEMSPOIL -v $POOL pool_hdr.checksum=0x0 >> $LOG

expect_abnormal_exit $PMEMPOOL check -vry $POOL >> $LOG
expect_normal_exit $PMEMPOOL check -avry $POOL >> $LOG
expect_normal_exit $PMEMPOOL check $POOL >> $LOG
expect_normal_exit $PMEMPOOL info $POOL 2>&1 > $null

check

pass
