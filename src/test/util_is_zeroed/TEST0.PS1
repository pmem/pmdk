# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018, Intel Corporation

#
# src/test/util_is_zeroed/TEST0 -- unit test for util_is_zeroed
#

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type none
require_build_type debug nondebug

setup

expect_normal_exit $Env:EXE_DIR\util_is_zeroed$Env:EXESUFFIX

pass
