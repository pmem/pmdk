# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019, Intel Corporation

#
# src/test/obj_pool/TEST16 -- unit test for pmemobj_create
#

. ..\unittest\unittest.ps1

require_test_type medium

setup

# non-existing file, layout length == PMEMOBJ_MAX_LAYOUT, poolsize > 0
$PMEMOBJ_MAX_LAYOUT = "x" * 1023
expect_normal_exit $Env:EXE_DIR\obj_pool$Env:EXESUFFIX `
    c $DIR\testfile $PMEMOBJ_MAX_LAYOUT 20 0600

check_files $DIR\testfile

check

pass
