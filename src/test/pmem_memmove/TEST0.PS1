# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation

#
# src/test/pmem_memmove/TEST0 -- unit test for pmem_memmove

#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type pmem non-pmem

setup

function test {
	remove_files $DIR\testfile1
	create_holey_file 4M $DIR\testfile1
	expect_normal_exit $Env:EXE_DIR\pmem_memmove$Env:EXESUFFIX $DIR\testfile1 $args
}

function test_all {
	# No offset, no overlap
	test b:4096

	# aligned dest, unaligned source, no overlap
	test s:7 b:4096

	# unaligned dest, unaligned source, no overlap
	test d:7 s:13 b:4096

	# all aligned, src overlaps dest
	test b:4096 s:23 o:1

	# unaligned destination
	test b:4096 d:21

	# unaligned source and dest
	test b:4096 d:21 s:7

	# overlap of src, aligned src and dest
	test b:4096 o:1 s:20

	# overlap of src, aligned src, unaligned dest
	test b:4096 d:13 o:1 s:20

	# dest overlaps src, unaligned dest, aligned src
	test b:2048 d:33 o:1

	# dest overlaps src, aligned dest and src
	test b:4096 o:1 d:20

	# aligned dest, no overlap, small length
	test b:8

	# small length, offset 1 byte from 64 byte boundary
	test b:4 d:63

	# overlap, src < dest, small length (ensures a copy backwards,
	# with number of bytes to align < length)
	test o:1 d:2 b:8
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
