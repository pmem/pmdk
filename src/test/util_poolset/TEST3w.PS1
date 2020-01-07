# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2019, Intel Corporation

#
# src/test/util_poolset/TEST3w -- unit test for util_pool_open()
#

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type non-pmem

setup

$Env:TEST_LOG_LEVEL = "4"
$Env:TEST_LOG_FILE = "test$Env:UNITTEST_NUM.log"

$MIN_POOL = 4 * 1024 * 1024 # 4MiB
$MIN_PART = (2 * 1024 * 1024).toString() + "B" # 2MiB

$FILES = @()
for ($p = 1; $p -le 128; $p++) {
    $FILES += "${MIN_PART}:$DIR\testfile${p}:x"
}

# prepare pool sets
create_poolset $DIR\testset1 @FILES

# create pool sets
expect_normal_exit $Env:EXE_DIR\util_poolset$Env:EXESUFFIX c $MIN_POOL `
    $DIR\testset1

expect_normal_exit $Env:EXE_DIR\util_poolset$Env:EXESUFFIX o $MIN_POOL `
    $DIR\testset1

sls -Path $Env:TEST_LOG_FILE -Pattern "<1>" | `
    %{[string]$_ -replace '^.* len ',"" -replace '^.*][ ]*',''} `
    > grep$Env:UNITTEST_NUM.log

check

pass
