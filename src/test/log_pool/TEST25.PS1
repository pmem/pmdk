# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation
#
# src/test/log_pool/TEST25 -- unit test for pmemlog_open
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type "any"
# icacls does have problems with handling long paths in the correct way.
require_short_path

setup

#
# TEST25 existing file, file size >= min required size
#        (no write permissions)
#
expect_normal_exit $Env:EXE_DIR\log_pool$Env:EXESUFFIX c $DIR\testfile 20 0600
# deny write
& icacls $DIR\testfile /deny ${Env:USERNAME}:W >$null

expect_normal_exit $Env:EXE_DIR\log_pool$Env:EXESUFFIX o $DIR\testfile

# restore full permissions
& icacls $DIR\testfile /grant ${Env:USERNAME}:F >$null
check

pass
