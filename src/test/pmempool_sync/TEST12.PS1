# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2019, Intel Corporation
#
#
# pmempool_sync/TEST12 -- test for checking pmempool sync
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

$LAYOUT = "OBJ_LAYOUT"
$POOLSET = "$DIR\pool0.set"

# Create poolset file
create_poolset $POOLSET `
    20M:$DIR\testfile1:x `
    20M:$DIR\testfile2:x `
    21M:$DIR\testfile3:x `
    R `
    40M:$DIR\testfile4:x `
    20M:$DIR\testfile5:x

# Create poolset
expect_normal_exit $PMEMPOOL create --layout=$LAYOUT obj $POOLSET
cat $LOG

# Corrupt metadata in primary replica, recalculate checksum
expect_normal_exit $PMEMSPOIL $DIR\testfile1 "pool_hdr.uuid=0000000000000000" `
    "pool_hdr.f:checksum_gen"

# Try to synchronize replicas
expect_abnormal_exit $PMEMPOOL sync $POOLSET 2>$null

mv -Force $LOG_TEMP $LOG

pass
