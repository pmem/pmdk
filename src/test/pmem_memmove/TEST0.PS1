#
# Copyright 2016-2019, Intel Corporation
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#
#     * Neither the name of the copyright holder nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

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
