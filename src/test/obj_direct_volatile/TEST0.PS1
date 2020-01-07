# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019, Intel Corporation

#
# src/test/obj_direct_volatile/TEST0 -- unit test for direct_volatile
#

[CmdletBinding(PositionalBinding=$false)]
Param(
    [alias("d")]
    $DIR = ""
    )

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

expect_normal_exit $Env:EXE_DIR\obj_direct_volatile$Env:EXESUFFIX $DIR\test0

pass
