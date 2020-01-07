# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# src/test/obj_list_move/TEST1 -- unit test for list_move
#

. ..\unittest\unittest.ps1

require_test_type medium
require_build_type debug

setup

create_holey_file 1M $DIR\testfile

expect_normal_exit $Env:EXE_DIR\obj_list$Env:EXESUFFIX $DIR\testfile `
	i:0:0 i:0:0 i:0:-1 i:0:-1 P:2 R:2 P:4 R:4 `
	m:0:1:0 P:2 R:2 P:4 R:4 `
	m:-1:1:0 P:2 R:2 P:4 R:4 `
	m:1:1:0 P:2 R:2 P:4 R:4 `
	m:0:1:-1 P:2 R:2 P:4 R:4

check

pass
