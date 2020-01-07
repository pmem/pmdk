# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation
#
# src/test/obj_basic_integration/TEST0 -- unit test for
# pmemobj APIs
#

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type any

setup

create_holey_file 16M $DIR\testfile1

expect_normal_exit $Env:EXE_DIR\obj_basic_integration$Env:EXESUFFIX `
    $DIR\testfile1 `

check

pass
