# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018, Intel Corporation
#
#
# pmemobjcli/TEST2 -- test for pmemobjcli -p option
#

. ..\unittest\unittest.ps1

require_test_type medium

setup

$PFX = "..\tools\pmemobjcli\pmemobjcli"
$LOG = "out${Env:UNITTEST_NUM}.log"

expect_normal_exit $PMEMOBJCLI$EXESUFFIX -s ${PFX}_example.posc -p > $LOG
echo "---" >> $LOG
expect_normal_exit $PMEMOBJCLI$EXESUFFIX -s ${PFX}_example_tx.posc -p >> $LOG

check

pass
