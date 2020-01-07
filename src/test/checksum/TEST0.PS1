# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation
#
# src/test/checksum/TEST0 -- unit test for checksum
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type none

setup

expect_normal_exit $Env:EXE_DIR\checksum$Env:EXESUFFIX .\file1 .\file2 `
    .\file3 .\file4 .\file5

check

pass
