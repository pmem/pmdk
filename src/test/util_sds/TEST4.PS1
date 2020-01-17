# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2020, Intel Corporation

#
# src/test/util_sds/TEST4 -- unittest for shutdown state
#

. ..\unittest\unittest.PS1

require_test_type medium

require_fs_type any

require_build_type debug

setup

$Env:PMEM2_LOG_LEVEL=2
$Env:PMEM2_LOG_FILE=".\test$Env:UNITTEST_NUM.log"

# fail before setting open flag
expect_normal_exit $Env:EXE_DIR\util_sds 1 4 `
				   $DIR\testfile1 5 12 `
				   $DIR\testfile2 7 156 `
				   $DIR\testfile3 9 134

expect_normal_exit $Env:EXE_DIR\util_sds 0 0 `
				   $DIR\testfile1 5 12 `
				   $DIR\testfile2 7 156 `
				   $DIR\testfile3 9 134

Get-Content test$Env:UNITTEST_NUM.log | Where-Object {$_ -match 'shutdown_state_check'} > grep$Env:UNITTEST_NUM.log

check

pass
