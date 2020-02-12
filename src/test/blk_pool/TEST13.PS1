# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation
#
# src/test/blk_pool/TEST13 -- unit test for pmemblk_create
#

. ..\unittest\unittest.ps1

require_test_type medium

setup

#
# TEST13 non-existing file, poolsize >= min required size, bsize < min bsize
#
expect_normal_exit $Env:EXE_DIR\blk_pool$Env:EXESUFFIX `
    c $DIR\testfile 10 20 0600

check_files $DIR\testfile

check

pass
