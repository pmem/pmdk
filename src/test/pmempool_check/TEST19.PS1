# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2018, Intel Corporation
#
#
# pmempool_check/TEST16 -- test for checking and fixing pool files arch_flags
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

create_poolset $POOLSET 20M:$POOL_P1 20M:$POOL_P2
expect_normal_exit $PMEMPOOL create log $POOLSET

$INVALID_ARCH_FLAGS="FFFFFFFF"

# If valid part exists it can be used to check and fix invalid arch_flags
&$PMEMSPOIL -v $POOL_P1 "pool_hdr.arch_flags=$INVALID_ARCH_FLAGS" >> $LOG
expect_abnormal_exit $PMEMPOOL check -v $POOLSET >> $LOG
expect_normal_exit $PMEMPOOL check -vry $POOLSET >> $LOG
# pmempool info should exit normally
expect_normal_exit $PMEMPOOL info $POOLSET >> $null

# if valid part does not exist it is impossible to check arch_flags
&$PMEMSPOIL -v $POOL_P1 "pool_hdr.arch_flags=$INVALID_ARCH_FLAGS" >> $LOG
&$PMEMSPOIL -v $POOL_P2 "pool_hdr.arch_flags=$INVALID_ARCH_FLAGS" >> $LOG
expect_abnormal_exit $PMEMPOOL check -v $POOLSET >> $LOG
expect_abnormal_exit $PMEMPOOL check -vry $POOLSET >> $LOG
# pmempool check would fix checksum but pmempool info should fail
# because arch_flags is still invalid
expect_normal_exit $PMEMPOOL check -vrya $POOLSET >> $LOG
expect_abnormal_exit $PMEMPOOL info $POOLSET >> $LOG 2> $null

check

pass
