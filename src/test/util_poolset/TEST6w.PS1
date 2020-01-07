# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2019, Intel Corporation

#
# src/test/util_poolset/TEST6w -- unit test for util_pool_open()
#

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type non-pmem

setup

$Env:TEST_LOG_LEVEL = "6"
$Env:TEST_LOG_FILE = "test$Env:UNITTEST_NUM.log"

$MIN_POOL = 128 * 1024 * 1024 # 4MiB
$MIN_POOL_STR = ${MIN_POOL}.toString() + "B" # 128MiB
$RESVSIZE=260 * 1024 * 1024 # 260 MiB
$RESVSIZE_STR = ${RESVSIZE}.toString() + "B" # 260 MiB

$REQUIRE_POOL = ${MIN_POOL} * 8
require_free_space "${REQUIRE_POOL}b"

# prepare pool sets
create_poolset $DIR\testset2 `
	${RESVSIZE_STR}:$DIR\testdir21:d ${RESVSIZE_STR}:$DIR\testfile22
create_poolset $DIR\testset3 `
	${RESVSIZE_STR}:$DIR\testdir31:d R ${RESVSIZE_STR}:$DIR\testfile32
create_poolset $DIR\testset4 `
	${RESVSIZE_STR}:$DIR\testdir41:d ${RESVSIZE_STR}:$DIR\testdir42:d `
	R ${RESVSIZE_STR}:$DIR\testdir43:d ${RESVSIZE_STR}:$DIR\testfile44

create_poolset $DIR\testset6 `
	${RESVSIZE_STR}:$DIR\testdir61:d ${RESVSIZE_STR}:$DIR\testfile62 `
	O SINGLEHDR
create_poolset $DIR\testset7 `
	${RESVSIZE_STR}:$DIR\testdir71:d R ${RESVSIZE_STR}:$DIR\testfile72 `
	O SINGLEHDR
create_poolset $DIR\testset8 `
	${RESVSIZE_STR}:$DIR\testdir81:d ${RESVSIZE_STR}:$DIR\testdir82:d `
	R ${RESVSIZE_STR}:$DIR\testdir83:d ${RESVSIZE_STR}:$DIR\testfile84 `
	O SINGLEHDR

create_poolset $DIR\testset9 `
	${RESVSIZE_STR}:$DIR\testdir91 ${RESVSIZE_STR}:$DIR\testfile92:d `
	O SINGLEHDR

# create pool sets
expect_normal_exit $Env:EXE_DIR\util_poolset$Env:EXESUFFIX c $MIN_POOL `
    $DIR\testset2 `
    $DIR\testset3 `
    $DIR\testset4 `
    $DIR\testset6 `
    $DIR\testset7 `
    $DIR\testset8 `
    $DIR\testset9

sls -Path $Env:TEST_LOG_FILE -Pattern "<1>" | `
    %{[string]$_ -replace '^.* len ',"" -replace '^.*][ ]*',''} `
    > grep$Env:UNITTEST_NUM.log

check

pass
