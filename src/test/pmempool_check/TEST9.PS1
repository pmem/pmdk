# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2019, Intel Corporation
#
# pmempool_check/TEST9 -- test for checking pmemobj pool
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type pmem non-pmem

setup

$POOL="$DIR\file.pool"
$LOG="out$Env:UNITTEST_NUM.log"
rm $LOG -Force -ea si

expect_normal_exit $PMEMPOOL create obj $POOL

expect_normal_exit $PMEMPOOL check -vyr $POOL >> $LOG

&$PMEMSPOIL -v $POOL `
	pool_hdr.major=0x0 `
	pool_hdr.features.compat=0xfe `
	pool_hdr.features.incompat=0xfe `
	pool_hdr.features.ro_compat=0xff `
	pool_hdr.shutdown_state.usc=0 `
	pool_hdr.shutdown_state.uuid=0 `
	'pool_hdr.shutdown_state.f:checksum_gen' `
	pool_hdr.unused=ERROR `
	>> $LOG

expect_abnormal_exit $PMEMPOOL check -vyr $POOL >> $LOG

check

pass
