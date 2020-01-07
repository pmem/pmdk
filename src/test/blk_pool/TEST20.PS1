# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation
#
# src/test/blk_pool/TEST20 -- unit test for pmemblk_create
#

. ..\unittest\unittest.ps1

require_test_type medium

setup

#
# TEST20 non-existing file, bsize == 0
#
expect_normal_exit $Env:EXE_DIR\blk_pool$Env:EXESUFFIX `
    o $DIR\testfile 0

check

pass
