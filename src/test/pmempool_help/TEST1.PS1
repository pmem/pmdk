# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2018, Intel Corporation
#
#
# pmempool_help/TEST1 -- test for pmempool help
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$LOG="out$Env:UNITTEST_NUM.log"

rm $LOG -Force -ea si

foreach($cmd in "info","dump","create","check") {
	rm help_${cmd}.log -Force -ea si
    rm ${cmd}_help.log -Force -ea si
	expect_normal_exit $PMEMPOOL help $cmd >> help_${cmd}.log
	expect_normal_exit $PMEMPOOL ${cmd} --help >> ${cmd}_help.log
	compare-object (get-content help_${cmd}.log) (get-content ${cmd}_help.log) >> $LOG
}

expect_normal_exit $PMEMPOOL help help >> $LOG

check

pass
