#
# Copyright 2014-2018, Intel Corporation
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
# pmempool_check/TEST6 -- test for checking pools
#
[CmdletBinding(PositionalBinding=$false)]
Param(
    [alias("d")]
    $DIR = ""
    )

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$POOLSET="$DIR\pool.set"
$POOL_PART1="$DIR\pool.part1"
$POOL_PART2="$DIR\pool.part2"

$LOG="out$Env:UNITTEST_NUM.log"
rm $LOG -Force -ea si

create_poolset $POOLSET 32M:$DIR\pool.part1:z 32M:$DIR\pool.part2
check_file $POOLSET

echo "PMEMLOG: pool_hdr" >> $LOG
expect_normal_exit $PMEMPOOL create log $POOLSET
check_files $POOL_PART1 $POOL_PART2
&$PMEMSPOIL -v $POOLSET pool_hdr.signature=ERROR >> $LOG
expect_abnormal_exit $PMEMPOOL check $POOLSET >> $LOG
expect_abnormal_exit $PMEMPOOL check $POOL_PART1 >> $LOG
expect_normal_exit $PMEMPOOL check $POOL_PART2 >> $LOG

echo "PMEMLOG: pmemlog" >> $LOG
rm $POOL_PART1, $POOL_PART2 -Force -ea si
expect_normal_exit $PMEMPOOL create log $POOLSET
check_files $POOL_PART1 $POOL_PART2
&$PMEMSPOIL -v $POOLSET pmemlog.start_offset=0 pmemlog.end_offset=0 >> $LOG
expect_abnormal_exit $PMEMPOOL check $POOLSET >> $LOG
expect_normal_exit $PMEMPOOL check $POOL_PART1 >> $LOG
expect_normal_exit $PMEMPOOL check $POOL_PART2 >> $LOG

echo "PMEMBLK: pool_hdr" >> $LOG
rm $POOL_PART1, $POOL_PART2 -Force -ea si
expect_normal_exit $PMEMPOOL create blk 512 $POOLSET
check_files $POOL_PART1 $POOL_PART2
&$PMEMSPOIL -v $POOLSET pool_hdr.signature=ERROR >> $LOG
expect_abnormal_exit $PMEMPOOL check $POOLSET >> $LOG
expect_abnormal_exit $PMEMPOOL check $POOL_PART1 >> $LOG
expect_normal_exit $PMEMPOOL check $POOL_PART2 >> $LOG

check

pass
