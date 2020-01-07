# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2018, Intel Corporation
#
#
# pmempool_info/TEST13 -- test for info command
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$POOL="$DIR\file.pool"
$LOG="out$Env:UNITTEST_NUM.log"
rm $LOG -Force -ea si

expect_normal_exit $PMEMPOOL create --layout pmempool$Env:SUFFIX obj $POOL
expect_normal_exit $PMEMALLOC -o $((3*1024*1024)) -t 1 $POOL
expect_normal_exit $PMEMALLOC -o 16 -t 2 $POOL
expect_normal_exit $PMEMPOOL info -O -E -a -A -H -Z -Cr0 -s $POOL >> $LOG

check

pass
