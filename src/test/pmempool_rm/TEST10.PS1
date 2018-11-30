#
# Copyright 2018, Intel Corporation
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#
#     * Neither the name of the copyright holder nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#
# pmempool_rm/TEST10.PS1 -- test for pmempool rm in interactive mode
#

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type any

setup

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
	$DIR\pool.obj

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
"@ | &$PMEMPOOL rm $DIR\pool.set

check_no_files $DIR\part2

check_files `
	$DIR\pool.part1 `
	$DIR\pool.set

pass
