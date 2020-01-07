# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation
#
# src/test/obj_basic_integration/TEST2 -- unit test for
# pmemobj APIs
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

create_poolset $DIR\testset1 8M:$DIR\testfile1:x 8M:$DIR\testfile2:x `
    r 16M:$DIR\testfile3:x

expect_normal_exit $Env:EXE_DIR\obj_basic_integration$Env:EXESUFFIX `
    $DIR\testset1 `

check

pass
