# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019, Intel Corporation

#
# src/test/pmem_unmap/TEST0 -- unit test for pmem_unmap
#

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type any

setup

create_holey_file 20M $DIR\testfile1

expect_normal_exit $Env:EXE_DIR\pmem_unmap$Env:EXESUFFIX $DIR\testfile1

check_files $DIR\testfile1

pass
