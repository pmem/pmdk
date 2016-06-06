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
# src/test/blk_nblock/TEST0 -- unit test for pmemblk_nblock
#
#
# parameter handling
#
[CmdletBinding(PositionalBinding=$false)]
Param(
    [alias("d")]
    $DIR = ""
    )
$Env:UNITTEST_NAME = "blk_nblock\TEST0w"
$Env:UNITTEST_NUM = "0w"
# XXX:  bash has a few calls to tools that we don't have on
# windows (yet) that set PMEM_IS_PMEM and NON_PMEM_IS_PMEM based
# on their output
$Env:PMEM_IS_PMEM = $true
$Env:NON_PMEM_IS_PMEM = $true

# standard unit test setup
. ..\unittest\unittest.ps1

setup

#
# Create a gajillion files, each file size created
# has six versions for the six block sizes being tested.
# Except for testfile1, since that should fail because
# it is too small.
#
# These are holey files, so they actually don't take up
# any significant space.
#
# If you run out of disk space you may have to comment
# out some of these temp files.  Even though they are
# sparse Windows still won't let you overcommit
#

create_holey_file 2 $DIR\testfile1
create_holey_file 2048 $DIR\testfile2.512
create_holey_file 2048 $DIR\testfile2.520
create_holey_file 2048 $DIR\testfile2.528
create_holey_file 2048 $DIR\testfile2.4096
create_holey_file 2048 $DIR\testfile2.4160
create_holey_file 2048 $DIR\testfile2.4224

#
# Larger file coverage is provided on the linux side
# we don't have the ability to test really large files
# with Windows
#

# MINIMUM POOL SIZE = 16MB + 8KB
$MIN_POOL_SIZE = "16+8"
create_holey_file $MIN_POOL_SIZE $DIR\testfile7.512
create_holey_file $MIN_POOL_SIZE $DIR\testfile7.520
create_holey_file $MIN_POOL_SIZE $DIR\testfile7.528
create_holey_file $MIN_POOL_SIZE $DIR\testfile7.4096
create_holey_file $MIN_POOL_SIZE $DIR\testfile7.4160
create_holey_file $MIN_POOL_SIZE $DIR\testfile7.4224

# should fail:
#   512:$DIR\testfile1 (file is too small)
#   4096:$DIR\testfile2.512 (bsize doesn't match pool)
expect_normal_exit ..\..\x64\debug\blk_nblock$Env:EXESUFFIX `
    512:$DIR\testfile1 `
    512:$DIR\testfile2.512 `
    4096:$DIR\testfile2.512 `
    520:$DIR\testfile2.520 `
    528:$DIR\testfile2.528 `
    4096:$DIR\testfile2.4096 `
    4160:$DIR\testfile2.4160 `

# check will print the appropriate pass/fail message
check
