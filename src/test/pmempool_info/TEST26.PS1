# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019, Intel Corporation
#
#
# pmempool_info/TEST26 -- test for info command
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$POOL="$DIR\file.pool"
$LOG="out$Env:UNITTEST_NUM.log"
rm $LOG -Force -ea si

expect_normal_exit $PMEMPOOL create --layout pmempool$Env:SUFFIX obj $POOL
&$PMEMSPOIL $POOL pool_hdr.shutdown_state.dirty=1 `
					"pool_hdr.shutdown_state.f:checksum_gen"
expect_normal_exit $PMEMPOOL info $POOL >> $LOG

check

pass
