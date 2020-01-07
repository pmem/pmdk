# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation

#
# src/test/obj_debug/TEST5 -- unit test for debug features
#

. ..\unittest\unittest.ps1

require_test_type medium
require_build_type debug
require_fs_type any

setup

$FUNCS=18

For ($i=0; $i -lt $FUNCS; $i++) {
    expect_abnormal_exit $Env:EXE_DIR\obj_debug$Env:EXESUFFIX `
    $DIR/testfile1 s $i 2>$null
}

pass
