#
# Copyright 2016-2018, Intel Corporation
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
# pmempool_sync/TEST5 -- test for checking pmempool sync;
#                        pmem/issues#275
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

# XXX: Not sure how to deal with the file permissions (chmod) on Windows.

#$LOG = "out${Env:UNITTEST_NUM}.log"
#$LOG_TEMP = "out${Env:UNITTEST_NUM}_part.log"
#rm $LOG -Force -ea si
#touch $LOG
#rm $LOG_TEMP -Force -ea si
#touch $LOG_TEMP

#$LAYOUT = "OBJ_LAYOUT${Env:SUFFIX}"
#$POOLSET = "$DIR\pool0.set"

## Create poolset file
#create_poolset $POOLSET `
#    20M:$DIR\testfile00:x `
#    30M:$DIR\testfile01:x `
#    R `
#    30M:$DIR\testfile10:x `
#    20M:$DIR\testfile11:x

## Create a poolset
#expect_normal_exit $PMEMPOOL create --layout=$LAYOUT obj $POOLSET
#cat $LOG >> $LOG_TEMP

## Delete the second part in the primary replica
#rm $DIR\testfile01 -Force -ea si

## Synchronize replicas
#expect_normal_exit $PMEMPOOL sync $POOLSET >> $LOG_TEMP

## Check if restored part file have the same permissions as other parts
#(Get-Item $DIR\testfile00).mode >> $LOG_TEMP
#(Get-Item $DIR\testfile01).mode >> $LOG_TEMP

## Delete the first part in the second replica
#rm $DIR\testfile10 -Force -ea si

# XXX: Need chmod() equivalent on Windows

## Change permissions of all the files
#chmod 600 $DIR\testfile*

## Synchronize replicas
#expect_normal_exit $PMEMPOOL sync $POOLSET >> $LOG_TEMP

## Check if restored part file have the same permissions as other parts
#(Get-Item $DIR\testfile10).mode >> $LOG_TEMP
#(Get-Item $DIR\testfile11).mode >> $LOG_TEMP

#mv -Force $LOG_TEMP $LOG
#check

pass
