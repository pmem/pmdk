# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# src/test/util_poolset/TEST1w -- unit test for util_pool_open()
#

. ..\unittest\unittest.ps1

require_test_type medium

setup

$Env:TEST_LOG_LEVEL = "4"
$Env:TEST_LOG_FILE = "test$Env:UNITTEST_NUM.log"

$MIN_POOL = 4 * 1024 * 1024 # 4MiB
$MIN_POOL_STR = ${MIN_POOL}.toString() + "B" # 4MiB

create_poolset $DIR\testset1 ${MIN_POOL_STR}:$DIR\testfile11:z:${MIN_POOL_STR} # pass
create_poolset $DIR\testset2 ${MIN_POOL_STR}:$DIR\testfile21:x `
    ${MIN_POOL_STR}:$DIR\testfile22:x # fail - can't read poolset file
create_poolset $DIR\testset3 ${MIN_POOL_STR}:$DIR\testfile31:x `
    ${MIN_POOL_STR}:$DIR\testfile32:x # fail - no files
create_poolset $DIR\testset4 ${MIN_POOL_STR}:$DIR\testfile41:z:${MIN_POOL_STR} `
    ${MIN_POOL_STR}:$DIR\testfile42:x # fail - no second part
create_poolset $DIR\testset5 1M:$DIR\testfile51:z:1M `
    ${MIN_POOL_STR}:$DIR\testfile52:z:${MIN_POOL_STR} # fail - part1 too small
create_poolset $DIR\testset6 ${MIN_POOL_STR}:$DIR\testfile61:z:${MIN_POOL_STR} `
    1M:$DIR\testfile62:z:1M # fail - part2 too small
create_poolset $DIR\testset7 2097151B:$DIR\testfile71:z:2097151B `
    ${MIN_POOL_STR}:$DIR\testfile72:z:${MIN_POOL_STR} # fail - part1 too small
create_poolset $DIR\testset8 ${MIN_POOL_STR}:$DIR\testfile81:z:${MIN_POOL_STR} `
    ${MIN_POOL_STR}:$DIR\testfile82:z:${MIN_POOL_STR} # fail - no access permissions
create_poolset $DIR\testset9 ${MIN_POOL_STR}:$DIR\testfile91:z:${MIN_POOL_STR} `
    512K:$DIR\testfile92:z:${MIN_POOL_STR} # fail - part2 size doesn't match
create_poolset $DIR\testset10 ${MIN_POOL_STR}:$DIR\testfile101:z:4194304B `
    ${MIN_POOL_STR}:$DIR\testfile102:z:4194305B # fail - part2 size doesn't match
create_poolset $DIR\testset11 2097153B:$DIR\testfile111:z:2097153B `
    2101247B:$DIR\testfile112:z:2101247B 2097152B:$DIR\testfile113:z:2097152B # pass

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
