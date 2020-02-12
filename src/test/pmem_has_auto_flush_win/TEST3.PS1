# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019, Intel Corporation

#
# src/test/pmem_has_auto_flush_win/TEST3 -- unit test for
#	pmem_has_auto_flush_win()
#
# case if capabilities value has improper bits
#
[CmdletBinding(PositionalBinding=$false)]
Param(
    [alias("d")]
    $DIR = ""
    )

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type any

setup

$NFIT_AVAILABLE="y"
$PLATFORM_CAPABILITIES=7
$CAPABILITIES=1

expect_normal_exit $Env:EXE_DIR\pmem_has_auto_flush_win$Env:EXESUFFIX `
	$NFIT_AVAILABLE $PLATFORM_CAPABILITIES $CAPABILITIES

check

pass
