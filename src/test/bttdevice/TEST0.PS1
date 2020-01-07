# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018, Intel Corporation
#
#
# bttdevice/TEST0 -- test for checking bttcreate tool
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$POOL="$DIR\file.pool"
$LOG="out${Env:UNITTEST_NUM}.log"
rm $LOG -Force -ea si
touch $LOG

echo "Create btt device - uuid defined by the user" >> $LOG
expect_normal_exit $BTTCREATE -s '20M' -b 1K -l 256 `
		-u "9047cad6-53e1-4ba8-bcdd-a9fb411f7392" -v $POOL >> $LOG

expect_normal_exit $PMEMPOOL info $POOL >> $LOG
check_file $POOL
rm $POOL -Force -ea si

echo "`nCreate btt device - automatic uuid generation" >> $LOG
expect_normal_exit $BTTCREATE -s 40M -b 2K -l 256 -v $POOL >> $LOG

expect_normal_exit $PMEMPOOL info $POOL >> $LOG
check_file $POOL

check

pass
