# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation
#
# src/test/blk_pool/TEST28 -- unit test for pmemblk_create
#

. ..\unittest\unittest.ps1

require_test_type medium

setup

#
# TEST28 existing file, file size >= min required size, bsize == 300
#
expect_normal_exit $Env:EXE_DIR\blk_pool$Env:EXESUFFIX `
    c $DIR\testfile 300 20 0640

expect_normal_exit $Env:EXE_DIR\blk_pool$Env:EXESUFFIX `
    o $DIR\testfile 300

check

pass
