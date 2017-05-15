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
# pmempool_transform/TEST5 -- test for checking pmempool transform;
#                             pmem/issues#262
#
[CmdletBinding(PositionalBinding=$false)]
Param(
    [alias("d")]
    $DIR = ""
    )
$Env:UNITTEST_NAME = "pmempool_transform\TEST5"
$Env:UNITTEST_NUM = "5w"

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type any

setup

$LOG = "out${Env:UNITTEST_NUM}.log"
$LOG_TEMP = "out${Env:UNITTEST_NUM}_part.log"
Remove-Item $LOG -Force -ea si
touch $LOG
Remove-Item $LOG_TEMP -Force -ea si
touch $LOG_TEMP

$LAYOUT = "OBJ_LAYOUT${Env:$SUFFIX}"
$POOLSET_IN = "$DIR/poolset.in"
$POOLSET_OUT = "$DIR/poolset.out"

# Create poolset files
create_poolset $POOLSET_IN `
    20M:$DIR/testfile00:x `
    20M:$DIR/testfile01:x `
    20M:$DIR/testfile02:x `
    R `
    60M:$DIR/testfile10:x

create_poolset $POOLSET_OUT `
    20M:$DIR/testfile00:x `
    20M:$DIR/testfile01:x `
    20M:$DIR/testfile02:x `
    R `
    90M:$DIR/abc/testfile10:x

# CLI script for writing some data hitting all the parts
$WRITE_SCRIPT = "$DIR/write_data"
Write-Output @"
pr 50M
srcp 0 TestOK111
srcp 25M TestOK222
srcp 45M TestOK333
"@ | out-file -encoding ASCII $WRITE_SCRIPT

# CLI script for reading 9 characters from all the parts
$READ_SCRIPT = "$DIR/read_data"
Write-Output @"
srpr 0 9
srpr 25M 9
srpr 45M 9
"@ | out-file -encoding ASCII $READ_SCRIPT

# Log poolset structures
Write-Output poolset_in >> $LOG_TEMP
Get-content $POOLSET_IN >> $LOG_TEMP
Write-Output "" >> $LOG_TEMP
Write-Output poolset_out >> $LOG_TEMP
Get-content $POOLSET_OUT >> $LOG_TEMP
Write-Output "" >> $LOG_TEMP

# Create a pool
expect_normal_exit $PMEMPOOL create --layout=$LAYOUT obj $POOLSET_IN
Get-Content $LOG >> $LOG_TEMP

# Write some data into the pool, hitting two part files
expect_normal_exit $PMEMOBJCLI -s $WRITE_SCRIPT $POOLSET_IN >> $LOG_TEMP

# Check if correctly written
expect_normal_exit $PMEMOBJCLI -s $READ_SCRIPT $POOLSET_IN >> $LOG_TEMP

# Try to transform the poolset
expect_abnormal_exit $PMEMPOOL transform `
    $POOLSET_IN $POOLSET_OUT >> $LOG_TEMP 2>$null

# Check if correctly read
expect_normal_exit $PMEMOBJCLI -s $READ_SCRIPT $POOLSET_IN >> $LOG_TEMP

# Check metadata by pmempool info
dump_pool_info $POOLSET_IN >> $LOG_TEMP
dump_pool_info $DIR/testfile00 >> $LOG_TEMP
dump_pool_info $DIR/testfile01 >> $LOG_TEMP
dump_pool_info $DIR/testfile02 >> $LOG_TEMP
dump_pool_info $DIR/testfile10 >> $LOG_TEMP

Move-Item -Force $LOG_TEMP $LOG
check

pass
