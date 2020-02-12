# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2018, Intel Corporation
#
#
# pmempool_rm/TEST2.PS1 -- test for pmempool rm
#

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type any
require_short_path

setup

$LOG="out$Env:UNITTEST_NUM.log"
remove_files $LOG

# Create poolset with replica
create_poolset $DIR\pool.set 32M:$DIR\pool.part1:x 32M:$DIR\pool.part2:x `
	R 32M:$DIR\rep.part1 32M:$DIR\rep.part2

# Check if pool set file exists
check_files `
	$DIR\pool.set

# Check if all pools don't exist
check_no_files `
	$DIR\pool.obj `
	$DIR\pool.part1 `
	$DIR\pool.part2 `
	$DIR\rep.part1 `
	$DIR\rep.part2

# Try to remove files without force flag
# all commands are called through cmd shell to avoid powershell stderror parsing
expect_abnormal_exit cmd /c $PMEMPOOL rm -v $DIR\pool.obj 2`>`&1  | out-file -literalpath $LOG
expect_abnormal_exit cmd /c $PMEMPOOL rm -sv $DIR\pool.set 2`>`&1 | out-file -append -literalpath $LOG

# Try to remove files with force flag
expect_normal_exit cmd /c $PMEMPOOL rm -flv `
	$DIR\pool.obj `
	$DIR\pool.set 2`>`&1 | out-file -append -literalpath $LOG

check_no_files `
	$DIR\pool.set `
	$DIR\pool.obj `
	$DIR\pool.part1 `
	$DIR\pool.part2 `
	$DIR\rep.part1 `
	$DIR\rep.part2

check

pass
