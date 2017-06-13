#
# Copyright 2017, Intel Corporation
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
# src/test/blk_pool/TEST31 -- create a pool with the smallest parts
# possible
#
#
# parameter handling
#
[CmdletBinding(PositionalBinding=$false)]
Param(
    [alias("d")]
    $DIR = ""
    )

$Env:UNITTEST_NAME = "blk_pool\TEST31w"
$Env:UNITTEST_NUM = "31w"


# standard unit test setup
. ..\unittest\unittest.ps1

require_test_type medium

setup

#
# TEST31 non-existing file, poolsize >= min required size, bsize > min bsize
#        part size == min part size (pool set)

create_poolset $DIR\testset 2M:$DIR\testfile1:x 2M:$DIR\testfile2:x `
    2M:$DIR\testfile3:x 2M:$DIR\testfile4:x 2M:$DIR\testfile5:x `
    2M:$DIR\testfile6:x 2M:$DIR\testfile7:x 2M:$DIR\testfile8:x `
    2M:$DIR\testfile9:x 2M:$DIR\testfile10:x

expect_normal_exit $Env:EXE_DIR\blk_pool$Env:EXESUFFIX `
    c $DIR\testset 100 0 0640

check_files $DIR\testset $DIR\testfile1 $DIR\testfile2

check

pass
