# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation
#
# src/test/obj_basic_integration/TEST3 -- unit test for
# pmemobj APIs
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

create_poolset $DIR\testset1 16M:$DIR\testfile1:x r 16M:$DIR\testfile2:x

expect_normal_exit $Env:EXE_DIR\obj_basic_integration$Env:EXESUFFIX `
    $DIR\testset1 `

compare_replicas "-soOaAb -l -Z -H -C" `
    $DIR\testfile1 $DIR\testfile2 > diff$Env:UNITTEST_NUM.log

check

pass
