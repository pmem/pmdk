# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2018, Intel Corporation
#
#
# pmempool_create/TEST8w.PS1 -- test for pmempool create command with poolset
#

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type any

setup

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
create_poolset $POOLREP 32M:${POOL1}:x 32M:${POOL2}:x 32M:${POOL3}:x `
                        R 32M:${REPL1}:z 32M:${REPL2}:z 32M:${REPL3}:z
check_file $POOLREP

#PMEMBLK
remove_files $POOLS
expect_normal_exit $PMEMPOOL create blk 512 $POOLSET
check_files $POOL1 $POOL2 $POOL3
check_signatures PMEMBLK $POOL1

# PMEMLOG
remove_files $POOLS
expect_normal_exit $PMEMPOOL create log $POOLSET
check_files $POOL1 $POOL2 $POOL3
check_signatures PMEMLOG $POOL1

# PMEMOBJ
remove_files $POOLS
expect_normal_exit $PMEMPOOL create --layout=pmempool$Env:SUFFIX obj $POOLSET
check_files $POOL1 $POOL2 $POOL3
check_signatures PMEMOBJ $POOL1

# PMEMOBJ with replica
remove_files $POOLS
expect_normal_exit $PMEMPOOL create --layout=pmempool$Env:SUFFIX obj $POOLREP
check_files $POOL1 $POOL2 $POOL3 $REPL1 $REPL2 $REPL3
check_signatures PMEMOBJ $POOL1 $REPL1

remove_files $POOLS $REPS $POOLSET $POOLREP

pass
