# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation

#
# src/test/pmem_memset/TEST0 -- unit test for pmem_memset
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type pmem non-pmem

setup

function test {
	remove_files $DIR\testfile1
	create_holey_file 4M $DIR\testfile1
	expect_normal_exit $Env:EXE_DIR\pmem_memset$Env:EXESUFFIX $DIR\testfile1 $args
}

function test_all {
	test 0 8
	test 13 4096
}

test_all

$Env:PMEM_AVX512F = 1

test_all

$Env:PMEM_AVX = 1

test_all

$Env:PMEM_NO_MOVNT = 1

test_all

$Env:PMEM_NO_GENERIC_MEMCPY = 1

test_all

pass
