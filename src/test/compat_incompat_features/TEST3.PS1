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
# compat_incompat_features/TEST3.PS1 -- test for compat/incompat feature flags
#
# check if pool fails to open when incompat flags differ between parts
#
[CmdletBinding(PositionalBinding=$false)]
Param(
    [alias("d")]
    $DIR = ""
    )
$Env:UNITTEST_NAME = "compat_incompat_features/TEST3"
$Env:UNITTEST_NUM = "3"

# standard unit test setup
. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type any

setup

. .\common.PS1

# Create poolset file
create_poolset $POOLSET `
    20M:$DIR\part0:x `
    20M:$DIR\part1:x

# Create poolset
expect_normal_exit $PMEMPOOL create --layout=$LAYOUT obj $POOLSET | out-file -append -encoding ascii -literalpath $LOG

# Set compat flag in part 0 header
set_incompat $DIR\part0 $POOL_FEAT_NOHDRS | out-file -append -encoding ascii -literalpath $LOG

# Check if pool opens
expect_abnormal_exit $PMEMPOOL info $POOLSET 2>&1 | out-file -append -encoding ascii -literalpath $LOG

# Set compat flag in part 0 header
set_incompat $DIR\part0 0 | out-file -append -encoding ascii -literalpath $LOG
set_incompat $DIR\part1 $POOL_FEAT_NOHDRS | out-file -append -encoding ascii -literalpath $LOG

# Check if pool opens
expect_abnormal_exit $PMEMPOOL info $POOLSET 2>&1 | out-file -append -encoding ascii -literalpath $LOG

# Set compat flag in part 0 header
set_incompat $DIR\part0 $UNKNOWN_INCOMPAT[0] | out-file -append -encoding ascii -literalpath $LOG

# Check if pool opens
expect_abnormal_exit $PMEMPOOL info $POOLSET 2>&1 | out-file -append -encoding ascii -literalpath $LOG

# Set compat flag in part 0 header
set_incompat $DIR\part1 $UNKNOWN_INCOMPAT[1] | out-file -append -encoding ascii -literalpath $LOG

# Check if pool opens
expect_abnormal_exit $PMEMPOOL info $POOLSET 2>&1 | out-file -append -encoding ascii -literalpath $LOG

pass
