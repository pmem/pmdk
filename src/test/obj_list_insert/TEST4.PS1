# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# src/test/obj_list_insert/TEST4 -- unit test for list_insert
#
# Adding item to before/after element in the middle
#

. ..\unittest\unittest.ps1

require_test_type medium
require_build_type debug

setup

create_holey_file 1M $DIR\testfile

expect_normal_exit $Env:EXE_DIR\obj_list$Env:EXESUFFIX $DIR\testfile `
	i:0:-1 i:0:-1 i:0:-1 P:2 R:2 `
	i:0:1 P:2 R:2 `
	i:1:2 P:2 R:2

check

pass
