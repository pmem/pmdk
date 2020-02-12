# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation
#
# src/test/util_is_absolute/TEST1 -- unit test for util_is_absolute_path
#
# NOTE: This is for Windows only!
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type none

setup

expect_normal_exit $Env:EXE_DIR\util_is_absolute$Env:EXESUFFIX `
    C:\foo\bar `
    \foo\bar `
    foo\bar `
    D:\foo\bar `
    /foo/bar `
    e: `
    E:\

check

pass
