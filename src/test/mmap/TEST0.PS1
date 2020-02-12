# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation
#
# src/test/mmap/TEST0 -- unit test for memory mapping routines
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

# create zero-length file
touch $DIR\testfile

expect_normal_exit $Env:EXE_DIR\mmap$Env:EXESUFFIX $DIR\testfile

pass
