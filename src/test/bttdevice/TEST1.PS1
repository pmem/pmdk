# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018, Intel Corporation
#
#
# bttdevice/TEST1 -- test for checking pmempool info for spoiled btt device
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$POOL="$DIR\file.pool"
$LOG="out${Env:UNITTEST_NUM}.log"
rm $LOG -Force -ea si
touch $LOG

expect_normal_exit $BTTCREATE -v $POOL >> $LOG

&$PMEMSPOIL -v $POOL "bttdevice.arena(0).btt_info.sig=BADSIGNATURE" `
		"bttdevice.arena(0).btt_info.external_lbasize=10" `
		"bttdevice.arena(0).btt_info.major=0x0" >> $LOG

expect_normal_exit $PMEMPOOL info $POOL -f btt >> $LOG
check_file $POOL

check

pass
