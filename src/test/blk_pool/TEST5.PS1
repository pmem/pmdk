# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation
#
# src/test/blk_pool/TEST5 -- unit test for pmemblk_create
#

. ..\unittest\unittest.ps1

require_test_type medium

setup

#
# TEST5 non-existing file, poolsize > 0
#       path is invalid (directory not exist)
#
expect_normal_exit $Env:EXE_DIR\blk_pool$Env:EXESUFFIX `
    c \nul\testfile 4096 20 0640

check_no_files $DIR\testfile

check

pass
