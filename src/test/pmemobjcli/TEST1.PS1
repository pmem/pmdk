# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2018, Intel Corporation
#
#
# pmemobjcli/TEST1 -- test for pmemobjcli with transactions
#

. ..\unittest\unittest.ps1

require_test_type medium

setup

$POOL = "$DIR\testfile"
$SCRIPT = "..\tools\pmemobjcli\pmemobjcli_example_tx.posc"
$LOG = "out${Env:UNITTEST_NUM}.log"

expect_normal_exit $PMEMPOOL create obj $POOL -s 16M
expect_normal_exit $PMEMOBJCLI -s $SCRIPT $POOL > $LOG

check

pass
