# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2020, Intel Corporation
#
# src/test/obj_pool_opem_mt/TEST0 -- unit test for pmemobj_pool
#

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type any

setup

expect_normal_exit $Env:EXE_DIR\obj_pool_open_mt$Env:EXESUFFIX $DIR 8

pass
