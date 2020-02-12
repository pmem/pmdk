# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# src/test/obj_list_macro/TEST2 -- unit test for list API
#

. ..\unittest\unittest.ps1

require_test_type medium

setup

expect_normal_exit $Env:EXE_DIR\obj_list_macro$Env:EXESUFFIX $DIR\testfile `
	i:0:0:1 P:1 R:1 `
	i:0:0:2 P:1 R:1 `
	i:0:-1:3 P:1 R:1 `
	i:0:-2:4 P:1 R:1 `
	i:0:-3:5 P:1 R:1 `
	i:0:-4:6 P:1 R:1 `
	i:0:-5:7 P:1 R:1 `
	i:0:-6:8 P:1 R:1 `
	i:0:-7:9 P:1 R:1 `
	i:0:-8:10 P:1 R:1

check

pass
