# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# src/test/pmem_memcpy/TEST0 -- unit test for pmem_memcpy
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

function test {
	remove_files $DIR\testfile1
	create_holey_file 4M $DIR\testfile1
	expect_normal_exit $Env:EXE_DIR\pmem_memcpy$Env:EXESUFFIX $DIR\testfile1 $args
}

function test_all {
	# aligned everything
	test 0 0 4096

	# unaligned dest
	test 7 0 4096

	# unaligned dest, unaligned src
	test 7 9 4096

	# aligned dest, unaligned src
	test 0 9 4096
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
