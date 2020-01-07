# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2019, Intel Corporation

#
# src/test/util_file_create/TEST0 -- unit test for util_file_create()
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

create_holey_file 32K $DIR\testfile1

mkdir $DIR\testdir1 > $null

expect_normal_exit $Env:EXE_DIR\util_file_create$Env:EXESUFFIX 0x4000 `
    0x4000:$DIR\testdir1 `
    0x4000:NUL `
    0x4000:$DIR\testfile1

check

pass
