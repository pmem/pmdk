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
# pmempool_check/TEST10 -- test for checking pools with multiple replicas
#
[CmdletBinding(PositionalBinding=$false)]
Param(
    [alias("d")]
    $DIR = ""
    )
$Env:UNITTEST_NAME = "pmempool_check\TEST10"
$Env:UNITTEST_NUM = "10"

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$POOLSET="$DIR/pool.set"
$POOL_R1_PART1="$DIR/pool.replica1.part1"
$POOL_R1_PART2="$DIR/pool.replica1.part2"
$POOL_R2_PART1="$DIR/pool.replica2.part1"
$POOL_R2_PART2="$DIR/pool.replica2.part2"

$LOG="out$Env:UNITTEST_NUM.log"
Remove-Item $LOG -Force -ea si

create_poolset $POOLSET `
	20M:$POOL_R1_PART1 50M:$POOL_R1_PART2 `
	R `
	40M:$POOL_R2_PART1 20M:$POOL_R2_PART2

expect_normal_exit $PMEMPOOL create obj $POOLSET
check_files $POOL_R1_PART1 $POOL_R1_PART2 $POOL_R2_PART1 $POOL_R2_PART2
&$PMEMSPOIL -v $POOL_R2_PART1 pool_hdr.uuid=ERROR >> $LOG
expect_abnormal_exit $PMEMPOOL check -vry $POOLSET >> $LOG
expect_abnormal_exit $PMEMPOOL info $POOLSET 2> $null

check

pass
