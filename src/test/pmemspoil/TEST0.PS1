# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2018, Intel Corporation
#
#
# pmemspoil/TEST0 -- test for pmemspoil
#

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type pmem non-pmem

setup

$LOG = "out${Env:UNITTEST_NUM}.log"

rm $LOG -Force -ea si
touch $LOG

$POOLSET = "$DIR\pool.set"
$POOL1 = "$DIR\pool.part1"
$POOL2 = "$DIR\pool.part2"
$POOL3 = "$DIR\pool.part3"
$REPL1 = "$DIR\pool.rep.part1"
$REPL2 = "$DIR\pool.rep.part2"
$REPL3 = "$DIR\pool.rep.part3"
$POOLS = "$POOL1 $POOL2 $POOL3 $REPL1 $REPL2 $REPL3"

rm $POOLS -Force -ea si
create_poolset $POOLSET 32M:${POOL1}:z 32M:${POOL2}:z 32M:${POOL3}:z `
    R 32M:${REPL1}:z 32M:${REPL2}:z 32M:${REPL3}:z
check_file $POOLSET

rm $POOLS -Force -ea si
expect_normal_exit $PMEMPOOL create obj --layout pmempool $POOLSET

expect_normal_exit $PMEMSPOIL $POOLSET --replica 0 "pmemobj.layout=replica0"
expect_normal_exit $PMEMPOOL info --replica 0 $POOLSET -f obj `
    | sls '^Layout.*' | %{$_.Line} >> $LOG

expect_normal_exit $PMEMSPOIL $POOLSET --replica 1 "pmemobj.layout=replica1"
expect_normal_exit $PMEMPOOL info --replica 1 $POOLSET -f obj `
    | sls '^Layout.*' | %{$_.Line} >> $LOG

check

pass
