# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2018, Intel Corporation
#
#
# pmempool_info/TEST14 -- test for info command
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$POOL="$DIR\file.pool"
$LOG="out$Env:UNITTEST_NUM.log"
rm $LOG -Force -ea si

expect_normal_exit $PMEMPOOL create --layout pmempool$Env:SUFFIX obj $POOL
expect_abnormal_exit $PMEMALLOC -o 1 -t 1 -ea $POOL
expect_normal_exit $PMEMPOOL info -l -R -a $POOL >> $LOG
echo "" >> $LOG
expect_abnormal_exit $PMEMALLOC -o 1 -t 1 -s -es $POOL
expect_normal_exit $PMEMPOOL info -l -R $POOL >> $LOG
echo "" >> $LOG
expect_abnormal_exit $PMEMALLOC -o 1024 -t 1 -s -es $POOL
expect_normal_exit $PMEMPOOL info -l -R $POOL >> $LOG
echo "" >> $LOG
expect_abnormal_exit $PMEMALLOC -o 1 -t 1 -s -f -ef $POOL
expect_normal_exit $PMEMPOOL info -l -R -a $POOL >> $LOG

check

pass
