# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2019, Intel Corporation

#
# src/test/util_file_create/TEST1 -- unit test for util_file_create()
#
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any
require_no_superuser
# icacls does have problems with handling long paths in the correct way.
require_short_path

setup

mkdir $DIR\testdir1 > $null

# remove write permissions
& icacls $DIR/testdir1 /deny ${Env:USERNAME}:W >$null

expect_normal_exit $Env:EXE_DIR\util_file_create$Env:EXESUFFIX 0x4000 `
    0x4000:$DIR\testdir1\testfile

# grant full permissions so test code can cleanup
& icacls $DIR/testdir1 /grant ${Env:USERNAME}:F >$null

check

pass
