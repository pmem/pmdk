# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation

#
# src/test/util_uuid_generate/TEST0 -- unit test for util_uuid_generate.
# No uuid string specified.
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type none

setup

expect_normal_exit $Env:EXE_DIR\util_uuid_generate$Env:EXESUFFIX

pass
