# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation

#
# src/test/obj_memblock/TEST0 -- unit test for memblock interface
#

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type none

setup

expect_normal_exit $Env:EXE_DIR\obj_memblock$Env:EXESUFFIX

pass
