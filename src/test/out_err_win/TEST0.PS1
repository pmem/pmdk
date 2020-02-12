# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation

#
# src/test/out_err_win/TEST0 -- unit test for out_err()
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type none
require_build_type debug

setup

$Env:TRACE_LOG_LEVEL = 1
$Env:TRACE_LOG_FILE = ".\traces$Env:UNITTEST_NUM.log"

expect_normal_exit $Env:EXE_DIR\out_err_win$Env:EXESUFFIX

check

pass
