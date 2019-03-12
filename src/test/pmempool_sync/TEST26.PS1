#
# Copyright 2018-2019, Intel Corporation
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
# pmempool_sync/TEST26 -- test for checking pmempool sync;
#                        a case with no replica in the pool set;
#                        pmem/issues#817
#

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type any

setup

$LOG = "out${Env:UNITTEST_NUM}.log"
$LOG_TEMP = "out${Env:UNITTEST_NUM}_part.log"
rm $LOG -Force -ea si
touch $LOG
rm $LOG_TEMP -Force -ea si
touch $LOG_TEMP

$LAYOUT = "OBJ_LAYOUT${Env:SUFFIX}"
$POOLSET = "$DIR\pool0.set"

# Create poolset file
create_poolset $POOLSET `
    20M:$DIR\testfile1:x `
    20M:$DIR\testfile2:x

# Create poolset
expect_normal_exit $PMEMPOOL create --layout=$LAYOUT obj $POOLSET
cat $LOG >> $LOG_TEMP

# Synchronize healthy pool set - should fail as there are no replicas
expect_abnormal_exit $PMEMPOOL sync $POOLSET >> $LOG_TEMP 2>&1

# Corrupt metadata in primary replica
expect_normal_exit $PMEMSPOIL $DIR\testfile1 "pool_hdr.uuid=0000000000000000" `
    >> $LOG_TEMP

# Synchronize broken pool set - should fail as there are no other replicas
expect_abnormal_exit $PMEMPOOL sync $POOLSET >> $LOG_TEMP 2>&1

# Remove poolset
expect_normal_exit $PMEMPOOL rm -f $POOLSET

# Create pmemlog poolset
expect_normal_exit $PMEMPOOL create log $POOLSET
cat $LOG >> $LOG_TEMP

# Synchronize healthy pool set - should fail as there are no replicas
expect_abnormal_exit $PMEMPOOL sync $POOLSET >> $LOG_TEMP 2>&1

# Corrupt metadata in primary replica
expect_normal_exit $PMEMSPOIL $DIR\testfile1 "pool_hdr.uuid=0000000000000000" `
    >> $LOG_TEMP

# Synchronize broken pool set - should fail as there are no other replicas
expect_abnormal_exit $PMEMPOOL sync $POOLSET >> $LOG_TEMP 2>&1

mv -Force $LOG_TEMP $LOG
#check

pass
