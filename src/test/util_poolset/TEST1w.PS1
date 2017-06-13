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
# src/test/util_poolset/TEST1w -- unit test for util_pool_open()
#
[CmdletBinding(PositionalBinding=$false)]
Param(
    [alias("d")]
    $DIR = ""
    )
$Env:UNITTEST_NAME = "util_poolset/TEST1w"
$Env:UNITTEST_NUM = "1w"

# standard unit test setup
. ..\unittest\unittest.ps1

require_test_type medium

setup

$Env:TEST_LOG_LEVEL = "4"
$Env:TEST_LOG_FILE = "test$Env:UNITTEST_NUM.log"

$MIN_POOL = 4 * 1024 * 1024 # 4MB

create_poolset $DIR\testset1 4M:$DIR\testfile11:z:4M # pass
create_poolset $DIR\testset2 4M:$DIR\testfile21:x `
    4M:$DIR\testfile22:x # fail - can't read poolset file
create_poolset $DIR\testset3 4M:$DIR\testfile31:x `
    4M:$DIR\testfile32:x # fail - no files
create_poolset $DIR\testset4 4M:$DIR\testfile41:z:4M `
    4M:$DIR\testfile42:x # fail - no second part
create_poolset $DIR\testset5 128K:$DIR\testfile51:z:128K `
    4M:$DIR\testfile52:z:4M # fail - part1 too small
create_poolset $DIR\testset6 4M:$DIR\testfile61:z:4M `
    128K:$DIR\testfile62:z:128K # fail - part2 too small
create_poolset $DIR\testset7 2097151B:$DIR\testfile71:z:2097151B `
    4M:$DIR\testfile72:z:4M # fail - part1 too small
create_poolset $DIR\testset8 4M:$DIR\testfile81:z:4M `
    4M:$DIR\testfile82:z:4M # fail - no access permissions
create_poolset $DIR\testset9 4M:$DIR\testfile91:z:4M `
    512K:$DIR\testfile92:z:4M # fail - part2 size doesn't match
create_poolset $DIR\testset10 4M:$DIR\testfile101:z:4194304B `
    4M:$DIR\testfile102:z:4194305B # fail - part2 size doesn't match
create_poolset $DIR\testset11 2097153B:$DIR\testfile111:z:2097153B `
    3145729B:$DIR\testfile112:z:3145729B 3145727B:$DIR\testfile113:z:3145727B # pass

expect_normal_exit $Env:EXE_DIR\util_poolset$Env:EXESUFFIX o $MIN_POOL `
    $DIR\testset0 $DIR\testset1 `
    "-mo:$DIR\testset2" $DIR\testset2 `
    $DIR\testset3 $DIR\testset4 `
    $DIR\testset5 $DIR\testset6 `
    $DIR\testset7 `
    "-mo:$DIR\testfile82" $DIR\testset8 `
    $DIR\testset9 $DIR\testset10 `
    $DIR\testset11

check_files $DIR\testfile11 `
    $DIR\testfile51 $DIR\testfile52 `
    $DIR\testfile61 $DIR\testfile62 `
    $DIR\testfile71 $DIR\testfile72 `
    $DIR\testfile81 $DIR\testfile82 `
    $DIR\testfile91 $DIR\testfile92 `
    $DIR\testfile101 $DIR\testfile102 `
    $DIR\testfile111 $DIR\testfile112 $DIR\testfile113

check_no_files $DIR\testfile21 $DIR\testfile22 `
    $DIR\testfile31 $DIR\testfile32 `
    $DIR\testfile42

sls -Path $Env:TEST_LOG_FILE -Pattern "<1>" | `
    %{[string]$_ -replace '^.* len ',"" -replace '^.*][ ]*',''} `
    > grep$Env:UNITTEST_NUM.log

check

pass
