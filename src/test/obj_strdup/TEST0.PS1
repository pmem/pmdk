# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation
#
# src/test/obj_strdup/TEST0 -- unit test for pmemobj_strdup
#

. ..\unittest\unittest.ps1

require_test_type medium

setup

expect_normal_exit $Env:EXE_DIR\obj_strdup$Env:EXESUFFIX $DIR\testfile1

pass
