# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# src/test/obj_list_macro/TEST3 -- unit test for list API
#

. ..\unittest\unittest.ps1

require_test_type medium

setup

expect_normal_exit $Env:EXE_DIR\obj_list_macro$Env:EXESUFFIX $DIR\testfile `
	i:1:0:1 P:1 R:1 `
	i:1:0:2 P:1 R:1 `
	i:1:-1:3 P:1 R:1 `
	i:1:-2:4 P:1 R:1 `
	i:1:-3:5 P:1 R:1 `
	i:1:-4:6 P:1 R:1 `
	i:1:-5:7 P:1 R:1 `
	i:1:-6:8 P:1 R:1 `
	i:1:-7:9 P:1 R:1 `
	i:1:-8:10 P:1 R:1

check

pass
