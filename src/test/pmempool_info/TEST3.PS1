# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2018, Intel Corporation
#
#
# pmempool_info/TEST3 -- test for info command with BTT Info backup
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$POOL="$DIR\file.pool"
$LOG="out$Env:UNITTEST_NUM.log"
rm $LOG -Force -ea si

expect_normal_exit $PMEMPOOL create -w blk 512 $POOL
expect_normal_exit $PMEMPOOL info -B $POOL >> $LOG

check

pass
