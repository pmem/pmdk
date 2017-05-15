#
# Copyright 2015-2017, Intel Corporation
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
# src/test/util_poolset/TEST2w -- unit test for util_pool_open()
#
[CmdletBinding(PositionalBinding=$false)]
Param(
    [alias("d")]
    $DIR = ""
    )
$Env:UNITTEST_NAME = "util_poolset\TEST2w"
$Env:UNITTEST_NUM = "2w"

# standard unit test setup
. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type non-pmem

setup

$Env:TEST_LOG_LEVEL = "4"
$Env:TEST_LOG_FILE = "test$Env:UNITTEST_NUM.log"

$MIN_POOL = 256 * 1024  # 4 * mmap alignment

# prepare pool sets
create_poolset $DIR/testset1 256K:$DIR/testfile11:x 256K:$DIR/testfile12:x
create_poolset $DIR/testset2 256K:$DIR/testfile21:x 256K:$DIR/testfile22:x
create_poolset $DIR/testset3 256K:$DIR/testfile31:x 256K:$DIR/testfile32:x
create_poolset $DIR/testset4 256K:$DIR/testfile41:x 256K:$DIR/testfile42:x
create_poolset $DIR/testset5 256K:$DIR/testfile51:x 256K:$DIR/testfile52:x
create_poolset $DIR/testset6 256K:$DIR/testfile61:x 256K:$DIR/testfile62:x
create_poolset $DIR/testset7 256K:$DIR/testfile71:x 256K:$DIR/testfile72:x
create_poolset $DIR/testset8 256K:$DIR/testfile81:x 256K:$DIR/testfile82:x
create_poolset $DIR/testset9 256K:$DIR/testfile91:x 256K:$DIR/testfile92:x
create_poolset $DIR/testset10 256K:$DIR/testfile101:x 256K:$DIR/testfile102:x
create_poolset $DIR/testset11 256K:$DIR/testfile111:x 256K:$DIR/testfile112:x
create_poolset $DIR/testset12 256K:$DIR/testfile121:x 256K:$DIR/testfile122:x

# create pool sets
expect_normal_exit $Env:EXE_DIR/util_poolset$Env:EXESUFFIX c $MIN_POOL `
    $DIR/testset1 $DIR/testset2 `
    $DIR/testset3 $DIR/testset4 `
    $DIR/testset5 $DIR/testset6 `
    $DIR/testset7 $DIR/testset8 `
    $DIR/testset9 $DIR/testset10 `
    $DIR/testset11 $DIR/testset12

# inject some errors
&$PMEMSPOIL $DIR/testfile22 pool_hdr.checksum=0
&$PMEMSPOIL $DIR/testfile32 pool_hdr.signature="ERROR" "pool_hdr.checksum_gen()"
&$PMEMSPOIL $DIR/testfile42 pool_hdr.major=99 "pool_hdr.checksum_gen()"
&$PMEMSPOIL $DIR/testfile52 pool_hdr.arch_flags="0000000000000000" "pool_hdr.checksum_gen()"
&$PMEMSPOIL $DIR/testfile62 pool_hdr.compat_features=0x12345678 "pool_hdr.checksum_gen()"
&$PMEMSPOIL $DIR/testfile72 pool_hdr.compat_features=0x12345678 "pool_hdr.checksum_gen()"
&$PMEMSPOIL $DIR/testfile82 pool_hdr.compat_features=0x12345678 "pool_hdr.checksum_gen()"
&$PMEMSPOIL $DIR/testfile91 pool_hdr.poolset_uuid="0123456789012345" "pool_hdr.checksum_gen()"
&$PMEMSPOIL $DIR/testfile101 pool_hdr.next_part_uuid="0123456789012345" "pool_hdr.checksum_gen()"
&$PMEMSPOIL $DIR/testfile111 pool_hdr.prev_part_uuid="0123456789012345" `
    pool_hdr.next_part_uuid="0123456789012345" "pool_hdr.checksum_gen()"
&$PMEMSPOIL $DIR/testfile121 pool_hdr.next_repl_uuid="0123456789012345" "pool_hdr.checksum_gen()"

# now check if pool sets may be opened
expect_normal_exit $Env:EXE_DIR/util_poolset$Env:EXESUFFIX o $MIN_POOL `
    $DIR/testset1 $DIR/testset2 `
    $DIR/testset3 $DIR/testset4 `
    $DIR/testset5 $DIR/testset6 `
    $DIR/testset7 $DIR/testset8 `
    $DIR/testset9 $DIR/testset10 `
    $DIR/testset11 $DIR/testset12

Select-String -Path $Env:TEST_LOG_FILE -Pattern "<1>" | `
    ForEach-Object {[string]$_ -replace '^.* len ',"" -replace '^.*][ ]*',''} `
    > grep$Env:UNITTEST_NUM.log

check

pass
