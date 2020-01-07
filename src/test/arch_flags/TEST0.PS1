# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# src/test/arch_flags/TEST0 -- unit test for util_get_arch_flags function
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type none

setup

$Env:ARCH_FLAGS_LOG_LEVEL=15
$Env:ARCH_FLAGS_LOG_FILE="log$Env:UNITTEST_NUM.log"

expect_normal_exit $Env:EXE_DIR\arch_flags$Env:EXESUFFIX `
	0:0:0:0x0:0x0 `
	65535:0:0:0x0:0x0 `
	0:255:0:0x0:0x0 `
	0:0:255:0x0:0x0 `
	0:0:0:0x1:0x0 `
	0:0:0:0x0:0x1

check

pass
