# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2019, Intel Corporation

#
# src/test/util_file_open/TEST1 -- unit test for util_file_open()
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any
require_no_superuser
# icacls does have problems with handling long paths in the correct way.
require_short_path

setup

create_holey_file 32K $DIR\testfile1

# remove write permissions
& icacls $DIR\testfile1 /deny ${Env:USERNAME}:W >$null

create_holey_file 32K $DIR\testfile2

# remove write permissions
& icacls $DIR\testfile2 /deny ${Env:USERNAME}:W >$null

expect_normal_exit $Env:EXE_DIR\util_file_open$Env:EXESUFFIX 0x4000 `
    $DIR\testfile1 `
    $DIR\testfile2

# grant full permissions so test code can cleanup
& icacls $DIR\testfile1 /grant ${Env:USERNAME}:F >$null
& icacls $DIR\testfile2 /grant ${Env:USERNAME}:F >$null

check

pass
