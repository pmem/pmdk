# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019, Intel Corporation
#
#
# pmempool_rm/TEST12.PS1 -- test for pmempool rm - verify removing of
# a write-protected part file by answering with invalid response
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$LOG="out$Env:UNITTEST_NUM.log"

# Create poolset with replica
create_poolset $DIR\pool.set 32M:$DIR\pool.part1:z 32M:$DIR\pool.part2:z `
	R 32M:$DIR\rep.part1:z 32M:$DIR\rep.part2:z

set_file_mode IsReadOnly $true $DIR\pool.set $DIR\pool.part1 $DIR\pool.part2 $DIR\rep.part1 $DIR\rep.part2

# Check if all pools and poolset exist
check_files `
	$DIR\pool.set `
	$DIR\pool.part1 `
	$DIR\pool.part2 `
	$DIR\rep.part1 `
	$DIR\rep.part2

echo @"
x
x
\x03
"@ | &$PMEMPOOL rm -il $DIR\pool.part1 2>&1 | out-file -width 1024 -literalpath $LOG

check_exit_code

check_files `
	$DIR\pool.part1

check

pass
