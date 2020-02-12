# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# src/test/obj_persist_count/TEST0 -- unit test for persist count
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type non-pmem

setup

expect_normal_exit $Env:EXE_DIR\obj_persist_count$Env:EXESUFFIX $DIR\testfile

check

pass
