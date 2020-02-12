# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019, Intel Corporation
#
#
# pmempool_rm/TEST10.PS1 -- test for pmempool rm in interactive mode
#

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type any

setup

$LOG="out$Env:UNITTEST_NUM.log"

# Create pmemlog, pmemblk and pmemobj pools
expect_normal_exit $PMEMPOOL create log $DIR\pool.log
expect_normal_exit $PMEMPOOL create blk 512 $DIR\pool.blk
expect_normal_exit $PMEMPOOL create obj $DIR\pool.obj

# Create poolset
create_poolset $DIR\pool.set 32M:$DIR\pool.part1:z 32M:$DIR\pool.part2:z

# Create pmemobj pool from the poolset
expect_normal_exit $PMEMPOOL create obj $DIR\pool.set

# Check if files exist
check_files `
	$DIR\pool.set `
	$DIR\pool.part1 `
	$DIR\pool.part2 `
	$DIR\pool.log `
	$DIR\pool.blk `
	$DIR\pool.obj

set_file_mode IsReadOnly $true `
	$DIR\pool.set `
	$DIR\pool.part1 `
	$DIR\pool.part2 `
	$DIR\pool.log `
	$DIR\pool.blk `
	$DIR\pool.obj

# Try to remove all files in interactive mode
echo @"
n
N
Y
N
y
"@ | &$PMEMPOOL rm `
	$DIR\pool.set `
	$DIR\pool.log `
	$DIR\pool.blk `
	$DIR\pool.obj `
	> $LOG

check_no_files `
	$DIR\pool.log `
	$DIR\pool.obj

check_files `
	$DIR\pool.set `
	$DIR\pool.part1 `
	$DIR\pool.part2

echo @"
n
y
"@ | &$PMEMPOOL rm $DIR\pool.set >> $LOG

check_no_files $DIR\part2

check_files `
	$DIR\pool.part1 `
	$DIR\pool.set

pass
