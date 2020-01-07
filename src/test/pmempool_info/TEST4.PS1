# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2018, Intel Corporation
#
#
# pmempool_info/TEST4 -- test for info command with statistics
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$POOL="$DIR\file.pool"
$LOG="out$Env:UNITTEST_NUM.log"
rm $LOG -Force -ea si

expect_normal_exit $PMEMPOOL create -w blk 512 $POOL
expect_normal_exit $PMEMPOOL info -s $POOL >> $LOG

echo "">> $LOG

rm $POOL -Force -ea si
expect_normal_exit $PMEMPOOL create log $POOL
expect_normal_exit $PMEMPOOL info -s $POOL >> $LOG

check

pass
