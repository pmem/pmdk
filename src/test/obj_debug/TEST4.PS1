# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation

#
# src/test/obj_debug/TEST4 -- unit test for debug features
#

. ..\unittest\unittest.ps1

require_test_type medium
require_build_type debug
require_fs_type any

setup

expect_abnormal_exit $Env:EXE_DIR\obj_debug$Env:EXESUFFIX `
    $DIR/testfile1 n 2>$null

pass
