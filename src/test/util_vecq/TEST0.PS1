# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019, Intel Corporation

#
# src/test/util_vecq/TEST0 -- unit test for vecq implementation
#

. ..\unittest\unittest.ps1

require_test_type short

require_fs_type none

setup

expect_normal_exit $Env:EXE_DIR\util_vecq$Env:EXESUFFIX

pass
