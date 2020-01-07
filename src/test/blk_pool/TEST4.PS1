# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation
#
# src/test/blk_pool/TEST4 -- unit test for pmemblk_create
#

. ..\unittest\unittest.ps1

require_test_type medium

setup

Touch $DIR\testfile

#
# TEST4 existing file, file length < min required size, poolsize == 0
#
expect_normal_exit $Env:EXE_DIR\blk_pool$Env:EXESUFFIX `
    c $DIR\testfile 4096 0 0640

check_files $DIR\testfile

check

pass
