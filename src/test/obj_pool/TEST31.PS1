# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# src/test/obj_pool/TEST31 -- unit test for pmemobj_open
#

. ..\unittest\unittest.ps1

require_fs_type any
require_test_type medium

setup

#
# TEST31 existing file, file size > min required size, layout is NULL
#        (valid pool header), reopen after failure
#
expect_normal_exit $Env:EXE_DIR\obj_pool$Env:EXESUFFIX `
    c $DIR\testfile NULL 20 0640

expect_normal_exit $Env:EXE_DIR\obj_pool$Env:EXESUFFIX `
    f $DIR\testfile NULL

check

pass
