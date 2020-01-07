# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019, Intel Corporation

#
# src/test/obj_fragmentation/TEST0 -- unit test measures average
# heap fragmentation.
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type pmem

setup

expect_normal_exit $Env:EXE_DIR\obj_fragmentation$Env:EXESUFFIX 1024 `
		$DIR\testfile1

pass
