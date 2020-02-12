# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation
#
# src/test/blk_pool/TEST26 -- unit test for pmemblk_create
#

. ..\unittest\unittest.ps1

require_test_type medium
# icacls does have problems with handling long paths in the correct way.
require_short_path

setup

#
# TEST26 existing file, file size >= min required size, bsize == 0
#        (no write permissions)
#
expect_normal_exit $Env:EXE_DIR\blk_pool$Env:EXESUFFIX `
    c $DIR\testfile 512 20 0640

# remove write permissions
& icacls $DIR\testfile /deny ${Env:USERNAME}:W >$null

expect_normal_exit $Env:EXE_DIR\blk_pool$Env:EXESUFFIX `
    o $DIR\testfile 0

# grant full permissions so test code can cleanup
& icacls $DIR\testfile /grant ${Env:USERNAME}:F >$null

check

pass
