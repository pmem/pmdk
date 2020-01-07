# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2018, Intel Corporation
#
#
# pmempool_check/TEST18 -- test for fixing various fields repairability without
#  advanced flag
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$POOLSET="$DIR\pool.set"
$POOL_P1="$DIR\pool.p1"
$POOL_P2="$DIR\pool.p2"
$LOG="out$Env:UNITTEST_NUM.log"
rm $LOG -Force -ea si

create_poolset $POOLSET 20M:$POOL_P1 50M:$POOL_P2

expect_normal_exit $PMEMPOOL create log $POOLSET

&$PMEMSPOIL -v $POOL_P1 `
    "pool_hdr.major=0x0" `
    "pool_hdr.poolset_uuid=0x0" `
    "pool_hdr.uuid=0x0" `
    >> $LOG

expect_normal_exit $PMEMPOOL check -rvy $POOLSET >> $LOG
expect_normal_exit $PMEMPOOL info $POOLSET >> $null

check

pass
