# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2020, Intel Corporation
#

#
# src/test/util_ravl/TEST0 -- unit test for util_ravl interface
#

. ..\unittest\unittest.ps1

require_fs_type none
require_test_type medium

setup

expect_normal_exit $Env:EXE_DIR\util_ravl$Env:EXESUFFIX

pass
