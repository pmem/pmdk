# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2018, Intel Corporation
#
#
# pmempool_info/TEST16.PS1 -- test for info command
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$LOG="out$Env:UNITTEST_NUM.log"
$LOG_REP0="rep$Env:UNITTEST_NUM.0.log"
$LOG_REP1="rep$Env:UNITTEST_NUM.1.log"

remove_files $LOG_REP0
remove_files $LOG_REP1
remove_files $LOG

$POOLSET="$DIR\pool.set"
$POOL1="$DIR\pool.part1"
$POOL2="$DIR\pool.part2"
$POOL3="$DIR\pool.part3"
$POOLS="$POOL1 $POOL2 $POOL3"
$POOLREP="$DIR\rep.set"
$REPL1="$DIR\pool.rep.part1"
$REPL2="$DIR\pool.rep.part2"
$REPL3="$DIR\pool.rep.part3"
$REPS="$REPL1 $REPL2 $REPL3"

remove_files $POOLSET
create_poolset $POOLSET 32M:${POOL1}:z 32M:${POOL2}:z 32M:${POOL3}:z
check_file $POOLSET

remove_files $POOLREP
create_poolset $POOLREP 32M:${POOL1}:z 32M:${POOL2}:z 32M:${POOL3}:z `
			R 32M:${REPL1}:z 32M:${REPL2}:z 32M:${REPL3}:z
check_file $POOLREP

remove_files $POOLS
expect_normal_exit $PMEMPOOL create blk 512 $POOLSET
expect_normal_exit $PMEMPOOL info $POOLSET >> $LOG

remove_files $POOLS
expect_normal_exit $PMEMPOOL create log $POOLSET
expect_normal_exit $PMEMPOOL info $POOLSET >> $LOG

remove_files $POOLS $REPS
expect_normal_exit $PMEMPOOL create obj --layout pmempool$Env:SUFFIX $POOLREP
expect_normal_exit $PMEMPOOL info $POOLREP >> $LOG
expect_normal_exit $PMEMPOOL info --replica 0 $POOLREP `
| select-string -pattern "UUID" > $LOG_REP0
expect_normal_exit $PMEMPOOL info --replica 1 $POOLREP `
| select-string -pattern "UUID" > $LOG_REP1

"DIFF" >> $LOG
Compare-Object $(Get-Content $LOG_REP0) $(Get-Content $LOG_REP1) >> $LOG

check

pass
