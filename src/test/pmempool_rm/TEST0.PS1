# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2018, Intel Corporation
#
#
# pmempool_rm/TEST0 -- test for pmempool rm
#

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type any

setup

$LOG="out$Env:UNITTEST_NUM.log"
remove_files $LOG

# Create pmemlog, pmemblk and pmemobj pools
expect_normal_exit $PMEMPOOL create log $DIR\pool.log
expect_normal_exit $PMEMPOOL create blk 512 $DIR\pool.blk
expect_normal_exit $PMEMPOOL create obj $DIR\pool.obj

# Create poolset with replica
create_poolset $DIR\pool.set 32M:$DIR\pool.part1:z 32M:$DIR\pool.part2:z `
	R 32M:$DIR\rep.part1 32M:$DIR\rep.part2

# Create pmemobj pools from the poolset
expect_normal_exit $PMEMPOOL create obj $DIR\pool.set

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

# Remove all pool files without the poolset
expect_normal_exit $PMEMPOOL rm -sv `
	$DIR\pool.log `
	$DIR\pool.blk `
	$DIR\pool.obj `
	$DIR\pool.set >> $LOG

# Check if all pool files don't exist
check_no_files `
	$DIR\pool.log `
	$DIR\pool.blk `
	$DIR\pool.obj `
	$DIR\pool.part1 `
	$DIR\pool.part2 `
	$DIR\rep.part1 `
	$DIR\rep.part2

# Pool set file must exist
check_file $DIR\pool.set

# Create pmemobj pools from the poolset
expect_normal_exit $PMEMPOOL create obj $DIR\pool.set

# Remove all pool files and poolset
expect_normal_exit $PMEMPOOL rm -v $DIR\pool.set >> $LOG

# Check if all pool files don't exist
check_no_files `
	$DIR\pool.part1 `
	$DIR\pool.part2 `
	$DIR\rep.part1 `
	$DIR\rep.part2

# Check if poolset file exists
check_file $DIR\pool.set

check

pass
