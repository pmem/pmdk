# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation
#
# src/test/blk_pool/TEST6 -- unit test for pmemblk_create
#

. ..\unittest\unittest.ps1

require_test_type medium

setup

touch $DIR\testfile

#
# TEST6 existing file, file length < min required size, poolsize == 0,
#       bsize == 0
#
expect_normal_exit $Env:EXE_DIR\blk_pool$Env:EXESUFFIX `
    c \NULL\testfile 0 0 0640

check_files $DIR\testfile

check

pass
