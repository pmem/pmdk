#
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
# src/test/blk_pool/TEST0 -- unit test for pmemblk_create
#
#
# parameter handling
#
[CmdletBinding(PositionalBinding=$false)]
Param(
    [alias("d")]
    $DIR = ""
    )

$Env:UNITTEST_NAME = "blk_pool\TEST0"
$Env:UNITTEST_NUM = "0"
# XXX:  bash has a few calls to tools that we don't have on
# windows (yet) that set PMEM_IS_PMEM and NON_PMEM_IS_PMEM based
# on their output
$Env:PMEM_IS_PMEM = $true
$Env:NON_PMEM_IS_PMEM = $true

# standard unit test setup
. ..\unittest\unittest.ps1

setup

# Note: linux test reset default permissions and assures the library
# creates a file using the permissions given. The Windows test just
# checks that the permissions match. Note that the library's use
# of the Windows CRT file functions (_chmod, _stat, etc) limit
# permissions to R/W or RO

#
# TEST0 non-existing file, poolsize > 0
#
expect_normal_exit ..\..\x64\debug\blk_pool$Env:EXESUFFIX `
    c $DIR\testfile 4096 20 0600

check_files $DIR\testfile

# check will print the appropriate pass/fail message
check
