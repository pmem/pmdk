# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019, Intel Corporation

#
# src/test/obj_check/TEST6 -- unit test for pmemobj_check
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

# existing file, invalid layout
$LAYOUT = "Valid_layout"
$INVALID_LAYOUT = "Invalid_layout"
$SIZE = ((20*1024*1024))

expect_normal_exit $PMEMPOOL create -l $LAYOUT -s $SIZE -m 0600 `
    obj $DIR\testfile
expect_normal_exit $Env:EXE_DIR\obj_check$Env:EXESUFFIX $DIR\testfile `
    -l $INVALID_LAYOUT

check

pass
