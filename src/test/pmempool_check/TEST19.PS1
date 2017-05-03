#
# Copyright 2017, Intel Corporation
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
# pmempool_check/TEST16 -- test for checking and fixing pool files arch_flags
#
[CmdletBinding(PositionalBinding=$false)]
Param(
    [alias("d")]
    $DIR = ""
    )
$Env:UNITTEST_NAME = "pmempool_check\TEST19"
$Env:UNITTEST_NUM = "19"

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$POOLSET="$DIR\pool.set"
$POOL_P1="$DIR\pool.p1"
$POOL_P2="$DIR\pool.p2"
$LOG="out$Env:UNITTEST_NUM.log"
rm $LOG -Force -ea si

create_poolset $POOLSET 20M:$POOL_P1 20M:$POOL_P2
expect_normal_exit $PMEMPOOL create log $POOLSET

$INVALID_ARCH_FLAGS="FFFFFFFF"

# If valid part exists it can be used to check and fix invalid arch_flags
&$PMEMSPOIL -v $POOL_P1 "pool_hdr.arch_flags=$INVALID_ARCH_FLAGS" >> $LOG
expect_abnormal_exit $PMEMPOOL check -v $POOLSET >> $LOG
expect_normal_exit $PMEMPOOL check -vry $POOLSET >> $LOG
# pmempool info should exit normally
expect_normal_exit $PMEMPOOL info $POOLSET >> $null

# if valid part does not exist it is impossible to check arch_flags
&$PMEMSPOIL -v $POOL_P1 "pool_hdr.arch_flags=$INVALID_ARCH_FLAGS" >> $LOG
&$PMEMSPOIL -v $POOL_P2 "pool_hdr.arch_flags=$INVALID_ARCH_FLAGS" >> $LOG
expect_abnormal_exit $PMEMPOOL check -v $POOLSET >> $LOG
expect_abnormal_exit $PMEMPOOL check -vry $POOLSET >> $LOG
# pmempool check would fix checksum but pmempool info should fail
# because arch_flags is still invalid
expect_normal_exit $PMEMPOOL check -vrya $POOLSET >> $LOG
expect_abnormal_exit $PMEMPOOL info $POOLSET >> $LOG 2> $null

check

pass
