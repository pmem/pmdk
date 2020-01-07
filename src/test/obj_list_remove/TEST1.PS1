# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# src/test/obj_list_remove/TEST1 -- unit test for list_remove
#
# Add 4 elements to single list and remove all elements by removing
# the last one always.
#

. ..\unittest\unittest.ps1

require_test_type medium
require_build_type debug

setup

create_holey_file 1M $DIR\testfile

expect_normal_exit $Env:EXE_DIR\obj_list$Env:EXESUFFIX $DIR\testfile `
	i:1:0 i:1:0 i:1:0 i:1:0 P:2 R:2 `
	r:-1 P:2 R:2 `
	r:-1 P:2 R:2 `
	r:-1 P:2 R:2 `
	r:-1 P:2 R:2

check

pass
