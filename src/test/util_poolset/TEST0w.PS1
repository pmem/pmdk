#
# Copyright 2015-2019, Intel Corporation
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

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type non-pmem

setup

$Env:TEST_LOG_LEVEL = "4"
$Env:TEST_LOG_FILE = "test$Env:UNITTEST_NUM.log"

$MIN_POOL = 4 * 1024 * 1024 # 4MiB
$MIN_POOL_STR = ${MIN_POOL}.toString() + "B" # 4MiB

mkdir $DIR\subdir1 -Force > $null
mkdir $DIR\subdir2 -Force > $null

# non-existing files (delete on failure)
create_poolset $DIR\testset1 ${MIN_POOL_STR}:$DIR\testfile11:x # pass
create_poolset $DIR\testset2 ${MIN_POOL_STR}:$DIR\testfile21:x `
    ${MIN_POOL_STR}:$DIR\testfile22:x # pass
create_poolset $DIR\testset3 ${MIN_POOL_STR}:$DIR\testfile31:x `
    ${MIN_POOL_STR}:$DIR\testfile32:z:${MIN_POOL_STR} # pass
create_poolset $DIR\testset4 128K:$DIR\testfile41:x `
    ${MIN_POOL_STR}:$DIR\testfile42:x # fail - part1 too small
create_poolset $DIR\testset5 ${MIN_POOL_STR}:$DIR\testfile51:x `
    128K:$DIR\testfile52:x # fail - part2 too small
create_poolset $DIR\testset6 ${MIN_POOL_STR}:$DIR\testfile61:x `
    ${MIN_POOL_STR}:$DIR\nodir\testfile62:x # fail - part2 non-existing dir
create_poolset $DIR\testset7 ${MIN_POOL_STR}:$DIR\testfile71:x `
    ${MIN_POOL_STR}:\dummy\testfile72:x # fail - part2 can't write to dir
create_poolset $DIR\testset8 ${MIN_POOL_STR}:$DIR\testfile81:x `
    1G:$DIR\testfile82:x # fail - part2 no space left

# exiting files (do not delete on failure)
create_poolset $DIR\testset10 ${MIN_POOL_STR}:$DIR\testfile101:z `
    ${MIN_POOL_STR}:$DIR\testfile102:z:${MIN_POOL_STR}:0400 # fail - part2 read-only
create_poolset $DIR\testset11 ${MIN_POOL_STR}:$DIR\testfile111:z:${MIN_POOL_STR} `
    ${MIN_POOL_STR}:$DIR\testfile112:z 2M:$DIR\testfile113:z:${MIN_POOL_STR} # fail - part3 too small
create_poolset $DIR\testset12 ${MIN_POOL_STR}:$DIR\testfile121:z:${MIN_POOL_STR} `
    1M:$DIR\testfile122:z:1M # fail - part2 too small
create_poolset $DIR\testset13 1M:$DIR\testfile131:z:1M `
    ${MIN_POOL_STR}:$DIR\testfile132:z:${MIN_POOL_STR} # fail - part2 too small
create_poolset $DIR\testset14 ${MIN_POOL_STR}:$DIR\testfile141:z:${MIN_POOL_STR} `
    8M:$DIR\testfile142:z:${MIN_POOL_STR} # fail - part2 size doesn't match
create_poolset $DIR\testset15 ${MIN_POOL_STR}:$DIR\testfile151:z:4194304B `
    ${MIN_POOL_STR}:$DIR\testfile152:z:4194303B # fail - part2 size doesn't match
create_poolset $DIR\testset16 4194303B:$DIR\testfile161:z:4194304B `
    ${MIN_POOL_STR}:$DIR\testfile162:z:${MIN_POOL_STR} # fail - part1 too small
create_poolset $DIR\testset17 262145B:$DIR\testfile171:z `
    266239B:$DIR\testfile172:z 266240B:$DIR\testfile173:z # pass
create_poolset $DIR\testset18 3M:$DIR\subdir1\testfile181:z `
    3M:$DIR\subdir2\testfile182:z # pass

# mixed (some files exist, some don't)
create_poolset $DIR\testset20 ${MIN_POOL_STR}:$DIR\testfile201:x `
    ${MIN_POOL_STR}:$DIR\testfile202:n # fail - part2 non-zeroed file
create_poolset $DIR\testset21 ${MIN_POOL_STR}:$DIR\testfile21:x `
    ${MIN_POOL_STR}:$DIR\testfile22:x # fail - part2 valid hdr (reuse file from case #2)
create_poolset $DIR\testset22 ${MIN_POOL_STR}:$DIR\testfile221:x `
    ${MIN_POOL_STR}:$DIR\testfile222:h # fail - part2 zeroed hdr (rest is non-zeroed)
create_poolset $DIR\testset23 ${MIN_POOL_STR}:$DIR\testfile231:x `
    ${MIN_POOL_STR}:$DIR\testfile232:z # fail - can't read set file
create_poolset $DIR\testset24 ${MIN_POOL_STR}:$DIR\testfile241:x ${MIN_POOL_STR}:$DIR\testfile242:x `
    r 8M:$DIR\testfile243:x `
    r 6M:$DIR\testfile244:x 2M:$DIR\testfile245:x # pass - replicas
create_poolset $DIR\testset25 ${MIN_POOL_STR}:$DIR\testfile251:z ${MIN_POOL_STR}:$DIR\testfile252:x `
    r 3M:$DIR\testfile253:z # fail - replica too small
create_poolset $DIR\testset26 ${MIN_POOL_STR}:$DIR\testfile261:z 2M:$DIR\testfile262:z `
    r 8M:$DIR\testfile263 r 6M:$DIR\testfile264 # pass - pmem\non-pmem

$_1GB = 1024 * 1024 * 1024
$_6MB = 6 * 1024 * 1024

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
    "-mp:$_6MB" $DIR\testset26

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
