# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation

#
# src/test/util_uuid_generate/TEST1 -- unit test for util_uuid_generate.
# Valid uuid string specified.

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type none

setup

expect_normal_exit $Env:EXE_DIR\util_uuid_generate$Env:EXESUFFIX `
	563bb872-0d1d-441e-ac28-579c463be0a5 valid

check

pass
