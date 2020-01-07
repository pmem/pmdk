# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2018, Intel Corporation
#
#
# pmempool_sync/TEST10 -- test for checking pmempool sync;
#                         check for healthy replicas cycles
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
$POOLSET1 = "$DIR\poolset1"
$POOLSET2 = "$DIR\poolset2"

# Create poolset files
create_poolset $POOLSET1 `
	20M:$DIR\part00:x `
	20M:$DIR\part01:x

create_poolset $POOLSET2 `
	20M:$DIR\part00:x `
	20M:$DIR\part01:x `
	r `
	20M:$DIR\part10:x `
	20M:$DIR\part11:x

# Log poolset structures
echo poolset1: >> $LOG_TEMP
cat $POOLSET1 >> $LOG_TEMP
echo poolset2: >> $LOG_TEMP
cat $POOLSET2 >> $LOG_TEMP

# Create a poolset
expect_normal_exit $PMEMPOOL create --layout=$LAYOUT obj $POOLSET1
cat $LOG >> $LOG_TEMP

# Try to synchronize replicas using the second poolset
expect_abnormal_exit $PMEMPOOL sync $POOLSET2 >> $LOG_TEMP 2>$null

# Check metadata using pmempool info
dump_pool_info $POOLSET1 >> $LOG_TEMP

mv -Force $LOG_TEMP $LOG
check

pass
