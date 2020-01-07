# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019, Intel Corporation

#
# src/test/obj_pool/TEST17 -- unit test for pmemobj_create
#

. ..\unittest\unittest.ps1

require_test_type medium

setup

# non-existing file, poolsize == RANGE_OF_SIZE_T_AS_MB
$RANGE_OF_SIZE_T=[UINT64]::MaxValue
$RANGE_OF_SIZE_T_AS_MB=($RANGE_OF_SIZE_T/1024/1024)-1
expect_normal_exit $Env:EXE_DIR\obj_pool$Env:EXESUFFIX `
    c $DIR\testfile test$Env:SUFFIX $RANGE_OF_SIZE_T_AS_MB 0640

check_no_files $DIR\testfile

check

pass
