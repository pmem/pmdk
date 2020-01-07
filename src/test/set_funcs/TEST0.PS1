# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# src/test/set_funcs/TEST0 -- unit test for pmem*_set_funcs
#

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type any

setup

expect_normal_exit $Env:EXE_DIR\set_funcs$Env:EXESUFFIX $DIR\testfile $DIR

pass
