# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019, Intel Corporation

#
# src/test/obj_pool/TEST15 -- unit test for pmemobj_create
#

. ..\unittest\unittest.ps1

require_test_type medium

setup

# non-existing file, poolsize == PMEMOBJ_MIN_POOL - 1
$PMEMOBJ_MIN_POOL = 8
$PMEMOBJ_MIN_POOL_BELOW = $PMEMOBJ_MIN_POOL-1
expect_normal_exit $Env:EXE_DIR\obj_pool$Env:EXESUFFIX `
    c $DIR\testfile test$Env:SUFFIX $PMEMOBJ_MIN_POOL_BELOW 0640

check_no_files $DIR\testfile

check

pass
