# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation
#
# src/test/log_pool/TEST10 -- unit test for pmemlog_create
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type "any"
# icacls does have problems with handling long paths in the correct way.
require_short_path

setup

create_holey_file 20M $DIR\testfile

# deny write (make read only)
& icacls $DIR\testfile /deny ${Env:USERNAME}:W >$null

#
# TEST11 existing file, file length >= min required size, poolsize == 0,
#        (no write permissions)
#
expect_normal_exit $Env:EXE_DIR\log_pool$Env:EXESUFFIX c $DIR\testfile 0 0600

# restore full permissions
& icacls $DIR\testfile /grant ${Env:USERNAME}:F >$null

check_files $DIR\testfile

check

pass
