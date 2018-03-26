#!/usr/bin/env bash
#
# Copyright 2016-2018, Intel Corporation
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
# src/test/pmem_is_pmem_proc/TEST0 -- unit test for pmem_is_pmem
#
[CmdletBinding(PositionalBinding=$false)]
Param(
    [alias("d")]
    $DIR = ""
    )

# standard unit test setup
. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type none

setup

# two mm entries
#
# 7fff77430000 0x00010000
#	start at first mm entry, extends into second: should produce: 1
# 7fff77430000 0x80000000
#	exactly matches sum of both mm entries: should produce: 1
# 7fff77430000 0x20000
#	start at first mm entry, extends into second: should produce: 1
# 7fff77430000 0x10000
#	entirely encompassed by first mm entry: should produce: 1
# 7fff77440000 0x00010000
#	exactly matches second mm entry, should produce: 1
# 7fff77440000 0x80000000
#	extends past second mm entry, should produce: 0
# 7fff77440000 8192
#	entirely encompassed by second mm entry: should produce: 1
# 7fff77480000 10240
#	subset of second mm entry: should produce: 1
# 7fff77440000 0xffffffffffffffff
#	extends past second mm entry, should produce: 0
# 7fff77430008 1
#	entirely encompassed by first mm entry: should produce: 1
# 7fff7743fff0 32
#	start at first mm entry, extends into second: should produce: 1

expect_normal_exit $Env:EXE_DIR\pmem_is_pmem_proc$Env:EXESUFFIX `
	a 0x7fff77430000 0x00010000 MAP_SYNC `
	a 0x7fff77440000 0x7FFF0000 MAP_SYNC `
	`
	t 0x7fff77430000 0x7FFF0000 `
	t 0x7fff77430000 0x80000000 `
	t 0x7fff77430000 0x20000 `
	t 0x7fff77430000 0x10000 `
	t 0x7fff77440000 0x7FFF0000 `
	t 0x7fff77440000 0x80000000 `
	t 0x7fff77440000 8192 `
	t 0x7fff77480000 10240 `
	t 0x7fff77440000 0xffffffffffffffff `
	t 0x7fff77430008 1 `
	t 0x7fff7743FFF0 32

check

pass
