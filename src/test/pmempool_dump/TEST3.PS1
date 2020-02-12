# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2018, Intel Corporation
#
#
# pmempool_dump/TEST3 -- test for dump command
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$POOL="$DIR\file.pool"
$LOG="out$Env:UNITTEST_NUM.log"

rm $LOG -Force -ea si

expect_normal_exit $PMEMPOOL create blk 512 $POOL
expect_normal_exit $PMEMWRITE $POOL 0:w:TEST0
expect_normal_exit $PMEMPOOL dump -r-1 -o dump.log $POOL
cat dump.log | findstr "^00000000" | out-file -encoding ASCII -append -literalpath $LOG
expect_normal_exit $PMEMPOOL dump -r0 $POOL | findstr "^00000000" | out-file -encoding ASCII -append -literalpath $LOG

check

pass
