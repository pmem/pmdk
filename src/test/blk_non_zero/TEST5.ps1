﻿#
# Copyright 2015-2016, Intel Corporation
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
# src/test/blk_non_zero/TEST4 -- unit test for
# pmemblk_read/write/set_zero/set_error
#
$Env:UNITTEST_NAME = "blk_non_zero\TEST5"
$Env:UNITTEST_NUM = "5"
# Pl TODO:  bash has a few calls to tools that we don't have on
# windows (yet) that set PMEM_IS_PMEM and NON_PMEM_IS_PMEM based
# on their outpute
$Env:PMEM_IS_PMEM = $true
$Env:NON_PMEM_IS_PMEM = $true
$DIR = ""

# standard unit test setup
. ..\unittest\unittest.ps1

# doesn't make sense to run in local directory
require_fs_type pmem non-pmem

setup

# single arena and minimum pmemblk pool file case
$MIN_POOL_SIZE = $((16*1024*1024 + 8*1024))

#
# All reads to an unwritten block pool should return zeros.
# Block 32202 is out of range and should return EINVAL.
# Attempts to zero uninitialized blocks are nops (should succeed).
#

expect_normal_exit ..\..\x64\debug\blk_non_zero$EXESUFFIX 512 $DIR\testfile1 `
c $MIN_POOL_SIZE r:0 r:1 r:32201 r:32202 z:0 z:1 r:0

# check will print the appropriate pass/fail message
check
