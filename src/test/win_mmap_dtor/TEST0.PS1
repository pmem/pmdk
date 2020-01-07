# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019, Intel Corporation

#
# src/test/win_mmap_dtor/TEST0 -- unit test for win_mmap destructor
#

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type any

setup

create_holey_file 2M $DIR\testfile1

expect_normal_exit $Env:EXE_DIR\win_mmap_dtor$Env:EXESUFFIX $DIR\testfile1

check_files $DIR\testfile1

pass
