# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2018, Intel Corporation
#
#
# pmempool_rm/TEST1w.PS1 -- test for pmempool rm
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$LOG="out$Env:UNITTEST_NUM.log"
remove_files $LOG

# Create pmemlog, pmemblk and pmemobj pools
expect_normal_exit $PMEMPOOL create -m400 log $DIR\pool.log
expect_normal_exit $PMEMPOOL create -m400 blk 512 $DIR\pool.blk
expect_normal_exit $PMEMPOOL create -m400 obj $DIR\pool.obj

# Create poolset with replica
create_poolset $DIR\pool.set 32M:$DIR\pool.part1:z 32M:$DIR\pool.part2:z `
	R 32M:$DIR\rep.part1:z 32M:$DIR\rep.part2:z

# Create pmemobj pools from the poolset
expect_normal_exit $PMEMPOOL create -m400 obj $DIR\pool.set

set_file_mode IsReadOnly $true $DIR\pool.set $DIR\pool.part1 $DIR\pool.part2 $DIR\rep.part1 $DIR\rep.part2

# Check if all pools and poolset exist
check_files `
	$DIR\pool.log `
	$DIR\pool.blk `
	$DIR\pool.obj `
	$DIR\pool.set `
	$DIR\pool.part1 `
	$DIR\pool.part2 `
	$DIR\rep.part1 `
	$DIR\rep.part2

@"
n
n
n
y
y
y
y
n
"@ | &$PMEMPOOL rm -sv `
$DIR\pool.log $DIR\pool.blk $DIR\pool.obj $DIR\pool.set >> $LOG
check_exit_code

check_files `
	$DIR\pool.log `
	$DIR\pool.blk `
	$DIR\pool.obj `
	$DIR\pool.set `

check_no_files `
	$DIR\pool.part1 `
	$DIR\pool.part2 `
	$DIR\rep.part1 `
	$DIR\rep.part2

expect_normal_exit $PMEMPOOL create -m400 obj $DIR\pool.set

check_files `
	$DIR\pool.log `
	$DIR\pool.blk `
	$DIR\pool.obj `
	$DIR\pool.set `
	$DIR\pool.part1 `
	$DIR\pool.part2 `
	$DIR\rep.part1 `
	$DIR\rep.part2

expect_normal_exit $PMEMPOOL rm -flv `
	$DIR\pool.log `
	$DIR\pool.blk `
	$DIR\pool.obj `
	$DIR\pool.set >> $LOG

check_no_files `
	$DIR\pool.log `
	$DIR\pool.blk `
	$DIR\pool.obj `
	$DIR\pool.set `
	$DIR\pool.part1 `
	$DIR\pool.part2 `
	$DIR\rep.part1 `
	$DIR\rep.part2

check

pass
