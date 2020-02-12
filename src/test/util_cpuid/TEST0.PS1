# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# src/test/util_cpuid/TEST0 -- unit test for CPU features detection
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type none

setup

expect_normal_exit $Env:EXE_DIR\util_cpuid$Env:EXESUFFIX

pass
