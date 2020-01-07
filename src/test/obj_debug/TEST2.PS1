# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# src/test/obj_debug/TEST2 -- unit test for debug features
#

. ..\unittest\unittest.ps1

require_test_type medium
require_build_type debug
require_fs_type any

setup

$Env:PMEMOBJ_LOG_LEVEL = "4"
expect_normal_exit $Env:EXE_DIR\obj_debug$Env:EXESUFFIX $DIR\testfile1 a

sls -Path pmemobj$Env:UNITTEST_NUM.log -Pattern "_pobj_debug_notice" | `
    %{$_.Line} > grep$Env:UNITTEST_NUM.log

check

pass
