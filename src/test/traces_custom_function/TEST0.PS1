# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation

#
# src/test/traces_custom_function/TEST0 -- unit test for traces custom
# print function
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type none
require_build_type debug

setup

$Env:TRACE_LOG_LEVEL = 4

expect_normal_exit $Env:EXE_DIR\traces_custom_function$Env:EXESUFFIX p

check

pass
