# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# src/test/obj_pmalloc_basic/TEST0 -- unit test for pmalloc interface
#

. ..\unittest\unittest.ps1

require_test_type medium

setup

expect_normal_exit $Env:EXE_DIR\obj_pmalloc_basic$Env:EXESUFFIX

pass
