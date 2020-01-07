# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018, Intel Corporation
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
expect_normal_exit $PMEMPOOL rm $POOLSET

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
