# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation
#
# src/test/blk_pool_win/TEST0 -- unit test for pmemblk_create
#

. ..\unittest\unittest.ps1

require_test_type medium

setup

# Note: linux test reset default permissions and assures the library
# creates a file using the permissions given. The Windows test just
# checks that the permissions match. Note that the library's use
# of the Windows CRT file functions (_chmod, _stat, etc) limit
# permissions to R/W or RO

#
# TEST0 non-existing file, poolsize > 0
#
expect_normal_exit $Env:EXE_DIR\blk_pool_win$Env:EXESUFFIX `
    c $DIR\testfile 4096 20 0600

check_files $DIR\testfile

check

pass
