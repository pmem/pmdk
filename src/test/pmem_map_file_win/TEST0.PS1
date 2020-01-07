# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2019, Intel Corporation

#
# src/test/pmem_map_file_win/TEST0 -- unit test for pmem_map_file_win
#

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type any

setup

create_holey_file 2G $DIR\testfile1

# <path> <size> <flags> <mode> <use_mlen> <use_is_pmem>

expect_normal_exit $Env:EXE_DIR\pmem_map_file_win$Env:EXESUFFIX `
    $DIR\testfile1 0 - 0 1 1 `
    $DIR\testfile1 -1 - 0 1 1 `
    $DIR\testfile1 0 - 0 0 0 `
    $DIR\testfile1 0 X 0 0 0 `
    $DIR\testfile1 0 - 0644 1 1 `
    $DIR\testfile1 1024 - 0 1 1 `
    $DIR\testfile1 0 C 0 1 1 `
    $DIR\testfile1 0 E 0 1 1 `
    $DIR\testfile1 4096 T 0644 1 1 `
    $DIR\testfile1 4096 E 0644 1 1 `
    $DIR\testfile1 0 - 0644 1 1

check_files $DIR\testfile1

check

pass
