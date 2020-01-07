# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# src/test/obj_list_macro/TEST0 -- unit test for list API
#

. ..\unittest\unittest.ps1

require_test_type medium

setup

expect_normal_exit $Env:EXE_DIR\obj_list_macro$Env:EXESUFFIX $DIR\testfile `
	i:0:0:1 P:1 R:1 `
	i:0:0:2 P:1 R:1 `
	i:0:0:3 P:1 R:1 `
	i:0:2:4 P:1 R:1 `
	i:0:0:5 P:1 R:1 `
	i:0:3:6 P:1 R:1 `
	i:0:0:7 P:1 R:1 `
	i:0:4:8 P:1 R:1 `
	i:0:0:9 P:1 R:1 `
	i:0:5:10 P:1 R:1

check

pass
