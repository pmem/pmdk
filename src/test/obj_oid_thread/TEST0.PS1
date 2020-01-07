# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# src/test/obj_oid_thread/TEST0 -- unit test for pmemobj_oid
#

. ..\unittest\unittest.ps1

require_fs_type any
require_test_type medium

setup

expect_normal_exit $Env:EXE_DIR\obj_oid_thread$Env:EXESUFFIX $DIR 9

pass
