# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation
#
# src/test/obj_pool_lookup/TEST0 -- unit test for pmemobj_pool
#

. ..\unittest\unittest.ps1

require_test_type medium

# doesn't make sense to run in local directory
require_fs_type any

setup

expect_normal_exit $Env:EXE_DIR\obj_pool_lookup$Env:EXESUFFIX $DIR 9

pass
