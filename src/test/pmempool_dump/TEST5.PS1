# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2018, Intel Corporation
#
#
# pmempool_dump/TEST5.PS1 -- test for dump command
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$POOL="$DIR\file.pool"
$LOG="out$Env:UNITTEST_NUM.log"
remove_files $LOG

$POOLSET="$DIR\pool.set"
$POOL_PART1="$DIR\pool.part1"
$POOL_PART2="$DIR\pool.part2"

remove_files $POOLSET $POOL_PART1 $POOL_PART2

create_poolset $POOLSET 32M:${POOL_PART1}:z 32M:${POOL_PART2}:z

expect_normal_exit $PMEMPOOL create log $POOLSET
expect_normal_exit $PMEMWRITE $POOLSET PMEMPOOL_DUMP_TEST
expect_normal_exit $PMEMPOOL dump $POOLSET >> $LOG

check

pass
