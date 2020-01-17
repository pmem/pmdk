# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2020, Intel Corporation

#
# src/test/util_sds/TEST0 -- unittest for shutdown state
#

. ..\unittest\unittest.PS1

require_test_type medium

require_fs_type any

require_build_type debug

setup

$Env:PMEM2_LOG_LEVEL=2
$Env:PMEM2_LOG_FILE=".\test$Env:UNITTEST_NUM.log"

# no error
expect_normal_exit $Env:EXE_DIR\util_sds 1 0 `
				   $DIR\testfile1 5 0 `
				   $DIR\testfile2 7 0 `
				   $DIR\testfile3 9 0

expect_normal_exit $Env:EXE_DIR\util_sds 0 0 `
				   $DIR\testfile1 5 0 `
				   $DIR\testfile2 7 0 `
				   $DIR\testfile3 9 0

Get-Content test$Env:UNITTEST_NUM.log | Where-Object {$_ -contains 'shutdown_state_check'} > grep$Env:UNITTEST_NUM.log

check

pass
