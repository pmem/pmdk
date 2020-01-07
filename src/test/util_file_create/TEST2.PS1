# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2019, Intel Corporation

#
# src/test/util_file_create/TEST2 -- unit test for util_file_create()
#
#

. ..\unittest\unittest.ps1

require_fs_type any
require_test_type medium

setup

expect_normal_exit $Env:EXE_DIR\util_file_create$Env:EXESUFFIX 0x4000 `
    0x7FFFFFFFFFFFFFFF:$DIR\testfile

check

pass
