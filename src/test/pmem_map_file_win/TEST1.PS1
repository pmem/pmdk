# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2019, Intel Corporation

#
# src/test/pmem_map_file_win/TEST1 -- unit test for pmem_map_file_win
#

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type any

setup

# <path> <size> <flags> <mode> <use_mlen> <use_is_pmem>

# pass the file of zero length
expect_normal_exit $Env:EXE_DIR\pmem_map_file_win$Env:EXESUFFIX `
    $DIR\testfile1 0 - 0666 1 1 `
    $DIR\testfile2 0 C 0666 0 0 `
    $DIR\testfile3 4096 C 0666 1 1 `
    $DIR\testfile4 0 E 0666 1 1 `
    $DIR\testfile5 4096 E 0666 1 1 `
    $DIR\testfile6 4096 CE 0666 1 1 `
    $DIR\testfile7 4096 CES 0666 1 1 `
    $DIR\testfile8 4096 CS 0666 1 1 `
    $DIR\testfile9 -1 C 0666 1 1 `
    $DIR\testfile10 4096 X 0666 1 1 `
    $DIR\testfile11 4096 CX 0666 1 1 `
    $DIR\testfile12 0x1FFFFFFFFFFFFFFF CE 0666 1 1 `
    $DIR 0 T 0666 1 1 `
    $DIR 4096 T 0666 1 1 `
    $DIR 4096 TC 0666 1 1 `
    $DIR 4096 TE 0666 1 1 `
    $DIR 4096 TS 0666 1 1 `
    $DIR 4096 TSE 0666 1 1 `
    $DIR 4096 TCE 0666 1 1 `
    $DIR 4096 TSEC 0666 1 1 `
    \nul\nonexistingdir 4096 T 0666 1 1

check_files $DIR\testfile3 `
    $DIR\testfile6 `
    $DIR\testfile7 `
    $DIR\testfile8

check_no_files $DIR\testfile1 `
    $DIR\testfile2 `
    $DIR\testfile4 `
    $DIR\testfile5 `
    $DIR\testfile9 `
    $DIR\testfile10 `
    $DIR\testfile11 `
    $DIR\testfile12

check

pass
