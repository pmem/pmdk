#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2022, Intel Corporation
#
# Copyright (c) 2016, Microsoft Corporation. All rights reserved.
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
# src/test/pmem_movnt/TEST0 -- unit test for pmem_memcpy, pmem_memmove
#                              and pmem_memset

. ..\unittest\unittest.ps1

require_test_type medium

require_build_type debug

setup

$Env:PMEM_IS_PMEM_FORCE=1
$Env:PMEM_LOG_LEVEL=15

function test {
	expect_normal_exit $Env:EXE_DIR\pmem_movnt$Env:EXESUFFIX
}

function test_all {
	$Env:PMEM_MOVNT_THRESHOLD=$null

	$Env:PMEM_MOVNT_THRESHOLD=1024

	$Env:PMEM_MOVNT_THRESHOLD=5

	$Env:PMEM_MOVNT_THRESHOLD=-15
}

test_all

$Env:PMEM_AVX512F = 0

test_all

$Env:PMEM_AVX = 0

test_all

$Env:PMEM_NO_MOVNT = 1
$Env:PMEM_NO_GENERIC_MEMCPY = 1

test

$Env:PMEM_MOVDIR64B = 1

test_all

pass
