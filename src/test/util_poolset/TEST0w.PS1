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
# src/test/util_poolset/TEST0w -- unit test for util_pool_create()
#
[CmdletBinding(PositionalBinding=$false)]
Param(
    [alias("d")]
    $DIR = ""
    )
$Env:UNITTEST_NAME = "util_poolset/TEST0w"
$Env:UNITTEST_NUM = "0w"

# standard unit test setup
. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type non-pmem

setup

$Env:TEST_LOG_LEVEL = "4"
$Env:TEST_LOG_FILE = "test$Env:UNITTEST_NUM.log"

$MIN_POOL = 256 * 1024  # 4 * mmap alignment

mkdir $DIR\subdir1 -Force > $null
mkdir $DIR\subdir2 -Force > $null

# non-existing files (delete on failure)
create_poolset $DIR\testset1 256K:$DIR\testfile11:x # pass
create_poolset $DIR\testset2 256K:$DIR\testfile21:x `
    256K:$DIR\testfile22:x # pass
create_poolset $DIR\testset3 256K:$DIR\testfile31:x `
    256K:$DIR\testfile32:z:256K # pass
create_poolset $DIR\testset4 12K:$DIR\testfile41:x `
    256K:$DIR\testfile42:x # fail - part1 too small
create_poolset $DIR\testset5 256K:$DIR\testfile51:x `
    16K:$DIR\testfile52:x # fail - part2 too small
create_poolset $DIR\testset6 256K:$DIR\testfile61:x `
    256K:$DIR\nodir\testfile62:x # fail - part2 non-existing dir
create_poolset $DIR\testset7 256K:$DIR\testfile71:x `
    256K:\dummy\testfile72:x # fail - part2 can't write to dir
create_poolset $DIR\testset8 256K:$DIR\testfile81:x `
    1G:$DIR\testfile82:x # fail - part2 no space left

# exiting files (do not delete on failure)
create_poolset $DIR\testset10 256K:$DIR\testfile101:z `
    256K:$DIR\testfile102:z:256K:0400 # fail - part2 read-only
create_poolset $DIR\testset11 256K:$DIR\testfile111:z:256K `
    256K:$DIR\testfile112:z 12K:$DIR\testfile113:z:256K # fail - part3 too small
create_poolset $DIR\testset12 256K:$DIR\testfile121:z:256K `
    8K:$DIR\testfile122:z:8K # fail - part2 too small
create_poolset $DIR\testset13 64K:$DIR\testfile131:z:64K `
    256K:$DIR\testfile132:z:256K # fail - part2 too small
create_poolset $DIR\testset14 256K:$DIR\testfile141:z:256K `
    512K:$DIR\testfile142:z:256K # fail - part2 size doesn't match
create_poolset $DIR\testset15 256K:$DIR\testfile151:z:262144B `
    256K:$DIR\testfile152:z:262145B # fail - part2 size doesn't match
create_poolset $DIR\testset16 262143B:$DIR\testfile161:z:262143B `
    256K:$DIR\testfile162:z:256K # fail - part1 too small
create_poolset $DIR\testset17 262145B:$DIR\testfile171:z `
    266239B:$DIR\testfile172:z 266240B:$DIR\testfile173:z # pass
create_poolset $DIR\testset18 259K:$DIR\subdir1\testfile181:z `
    257K:$DIR\subdir2\testfile182:z # pass

# mixed (some files exist, some don't)
create_poolset $DIR\testset20 256K:$DIR\testfile201:x `
    256K:$DIR\testfile202:n # fail - part2 non-zeroed file
create_poolset $DIR\testset21 256K:$DIR\testfile21:x `
    256K:$DIR\testfile22:x # fail - part2 valid hdr (reuse file from case #2)
create_poolset $DIR\testset22 256K:$DIR\testfile221:x `
    256K:$DIR\testfile222:h # fail - part2 zeroed hdr (rest is non-zeroed)
create_poolset $DIR\testset23 256K:$DIR\testfile231:x `
    256K:$DIR\testfile232:z # fail - can't read set file
create_poolset $DIR\testset24 256K:$DIR\testfile241:x 256K:$DIR\testfile242:x `
    r 280K:$DIR\testfile243:x `
    r 320K:$DIR\testfile244:x 256K:$DIR\testfile245:x # pass - replicas
create_poolset $DIR\testset25 256K:$DIR\testfile251:z 256K:$DIR\testfile252:x `
    r 64K:$DIR\testfile253:z # fail - replica too small
create_poolset $DIR\testset26 256K:$DIR\testfile261:z 512K:$DIR\testfile262:z `
    r 320K:$DIR\testfile263 r 768K:$DIR\testfile264 # pass - pmem\non-pmem

$_1GB = 1024*1024*1024
$_768KB = 768 * 1024

expect_normal_exit $Env:EXE_DIR\util_poolset$Env:EXESUFFIX c $MIN_POOL `
    $DIR\testset0 $DIR\testset1 `
    $DIR\testset2 $DIR\testset3 `
    $DIR\testset4 $DIR\testset5 `
    $DIR\testset6 `
    "-mo:\dummy\testfile72" $DIR\testset7 `
    "-mf:$_1GB" $DIR\testset8 `
    "-mo:$DIR\testfile102" $DIR\testset10 `
    $DIR\testset11 `
    $DIR\testset12 $DIR\testset13 `
    $DIR\testset14 $DIR\testset15 `
    $DIR\testset18 `
    $DIR\testset20 $DIR\testset21 `
    $DIR\testset22 `
    "-mo:$DIR\testset23" $DIR\testset23 `
    $DIR\testset24 $DIR\testset25 `
    "-mp:$_768KB" $DIR\testset26

check_files $DIR\testfile11 `
    $DIR\testfile21 $DIR\testfile22 `
    $DIR\testfile31 $DIR\testfile32 `
    $DIR\testfile101 $DIR\testfile102 `
    $DIR\testfile111 $DIR\testfile112 `
    $DIR\testfile121 $DIR\testfile122 `
    $DIR\testfile131 $DIR\testfile132 `
    $DIR\testfile141 $DIR\testfile142 `
    $DIR\testfile151 $DIR\testfile152 `
    $DIR\testfile161 $DIR\testfile162 `
    $DIR\testfile171 $DIR\testfile172 $DIR\testfile173 `
    $DIR\subdir1\testfile181 $DIR\subdir2\testfile182 `
    $DIR\testfile202 `
    $DIR\testfile221 $DIR\testfile222 `
    $DIR\testfile232 `
    $DIR\testfile241 $DIR\testfile242 $DIR\testfile243 $DIR\testfile244 $DIR\testfile245 `
    $DIR\testfile251 $DIR\testfile253 `
    $DIR\testfile261 $DIR\testfile262 $DIR\testfile263 $DIR\testfile264

check_no_files $DIR\testfile41 $DIR\testfile42 `
    $DIR\testfile51 $DIR\testfile52 `
    $DIR\testfile61 `
    $DIR\testfile81 $DIR\testfile82 `
    $DIR\testfile201 `
    $DIR\testfile231 `
    $DIR\testfile252

sls -Path $Env:TEST_LOG_FILE -Pattern "<1>" | `
    %{[string]$_ -replace '^.* len ',"" -replace '^.*][ ]*',''} `
    > grep$Env:UNITTEST_NUM.log

check

pass
