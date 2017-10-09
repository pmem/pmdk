#
# Copyright 2016-2017, Intel Corporation
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
# pmempool_info\TEST17.PS1 -- test for info command
#
[CmdletBinding(PositionalBinding=$false)]
Param(
    [alias("d")]
    $DIR = ""
    )
$Env:UNITTEST_NAME = "pmempool_info/TEST17"
$Env:UNITTEST_NUM = "17"


. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$LOG="out$Env:UNITTEST_NUM.log"
remove_files $LOG

$POOLSET="$DIR\pool.set"
$POOL1="$DIR\pool.part1"
$POOL2="$DIR\pool.part2"
$POOLS="$POOL1 $POOL2"
$REPL1="$DIR\pool.rep.part1"
$REPL2="$DIR\pool.rep.part2"
$REPS="$REPL1 $REPL2"

remove_files $POOLSET
create_poolset $POOLSET 32M:${POOL1}:z 32M:${POOL2}:z `
	R 32M:${REPL1}:z 32M:${REPL2}:z
check_file $POOLSET

remove_files $POOLS $REPS
expect_normal_exit $PMEMPOOL create obj --layout pmempool$Env:SUFFIX $POOLSET
expect_normal_exit $PMEMPOOL info $POOL1 >> $LOG
expect_normal_exit $PMEMPOOL info $POOL2 >> $LOG
expect_normal_exit $PMEMPOOL info $REPL1 >> $LOG
expect_normal_exit $PMEMPOOL info $REPL2 >> $LOG

check

pass
