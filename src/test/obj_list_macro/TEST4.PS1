# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# src/test/obj_list_macro/TEST4 -- unit test for list API
#

. ..\unittest\unittest.ps1

require_test_type medium

setup

expect_normal_exit $Env:EXE_DIR\obj_list_macro$Env:EXESUFFIX $DIR\testfile `
	n:0:0:1 P:1 P:2 `
	n:0:0:2 P:1 P:2 `
	n:0:0:3 P:1 P:2 `
	n:0:0:4 P:1 P:2 `
	n:0:0:5 P:1 P:2 `
	m:3:0:0 P:1 P:2 `
	m:2:0:0 P:1 P:2 `
	m:1:0:1 P:1 P:2 `
	n:0:0:6 P:1 P:2 `
	n:0:0:7 P:1 P:2 `
	m:1:0:1 P:1 P:2 `
	m:0:0:0 P:1 P:2 `
	m:0:0:-3 P:1 P:2 `
	m:0:0:0 P:1 P:2

check

pass
