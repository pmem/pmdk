# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2019, Intel Corporation

#
# src/test/util_sds/TEST5 -- unittest for shutdown state
#

. ..\unittest\unittest.PS1

require_test_type medium

require_fs_type any

require_build_type debug

setup

$Env:TEST_LOG_LEVEL=2
$Env:TEST_LOG_FILE=".\test$Env:UNITTEST_NUM.log"

# fail after an open flag is set
expect_normal_exit $Env:EXE_DIR\util_sds 1 5 `
				   $DIR\testfile1 5 0 `
				   $DIR\testfile2 7 0 `
				   $DIR\testfile3 9 0

expect_normal_exit $Env:EXE_DIR\util_sds 0 0 `
				   $DIR\testfile1 5 0 `
				   $DIR\testfile2 7 0 `
				   $DIR\testfile3 9 0

Get-Content test$Env:UNITTEST_NUM.log | Where-Object {$_ -match 'shutdown_state_check'} > grep$Env:UNITTEST_NUM.log

check

pass
