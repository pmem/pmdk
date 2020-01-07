# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2018, Intel Corporation
#
#
# pmempool_rm/TEST7.PS1 -- test for pmempool rm in interactive mode
#

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type any

setup

$LOG="out$Env:UNITTEST_NUM.log"
remove_files $LOG

# Create poolset with replica
create_poolset $DIR\pool.set 32M:$DIR\pool.part1:z 32M:$DIR\pool.part2:z `
	R 32M:$DIR\rep.part1:z 32M:$DIR\rep.part2:z

# Check if pool set file exists
check_files `
	$DIR\pool.set `
	$DIR\pool.part1 `
	$DIR\pool.part2 `
	$DIR\rep.part1 `
	$DIR\rep.part2

# Try to remove all files in interactive mode
echo @"
Y
n
n
y
N
"@ | &$PMEMPOOL rm -il $DIR\pool.set 2>&1 | out-file -width 1024 -literalpath $LOG

check_no_files `
	$DIR\pool.part1 `
	$DIR\rep.part2

check_files `
	$DIR\pool.set `
	$DIR\pool.part2 `
	$DIR\rep.part1

pass
