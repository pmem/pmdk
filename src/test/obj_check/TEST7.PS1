# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019, Intel Corporation

#
# src/test/obj_check/TEST7 -- unit test for pmemobj_check
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

# non-existing file
$LAYOUT = "Valid_layout"
$SIZE = ((20*1024*1024))

expect_normal_exit $Env:EXE_DIR\obj_check$Env:EXESUFFIX $DIR\testfile -l $LAYOUT

check

pass
