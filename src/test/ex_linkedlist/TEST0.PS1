# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation

#
# src/test/ex_linkedlist/TEST0 -- unit test for linkedlist example
#

. ..\unittest\unittest.ps1

require_test_type medium
require_build_type debug nondebug
require_no_unicode

setup

expect_normal_exit $Env:EXE_DIR\ex_linkedlist$Env:EXESUFFIX $DIR\testfile

check

pass
