# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation
#
# src/test/win_lists/TEST0 -- unit test for windows list macros
#

$DIR = ""

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type none

setup

expect_normal_exit $Env:EXE_DIR\win_lists$Env:EXESUFFIX

check

pass
