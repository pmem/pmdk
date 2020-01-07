# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2018, Intel Corporation
#
#
# pmempool_help/TEST0 -- test for pmempool help
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$LOG="out$Env:UNITTEST_NUM.log"

rm $LOG -Force -ea si

expect_normal_exit $PMEMPOOL >> $LOG
expect_normal_exit $PMEMPOOL --version >> $LOG
expect_normal_exit $PMEMPOOL --help >> $LOG

check

pass
