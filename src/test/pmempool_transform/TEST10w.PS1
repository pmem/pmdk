# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2018, Intel Corporation
#
#
# pmempool_transform/TEST10 -- test for checking pmempool transform;
#                             pmem/issues#285
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

$LAYOUT = "OBJ_LAYOUT${Env:$SUFFIX}"
$POOLSET_IN = "$DIR\poolset.in"
$POOLSET_OUT = "$DIR\poolset.out"

# Create poolset files
create_poolset $POOLSET_IN `
	20M:$DIR\testfile00:x `
	20M:$DIR\testfile01:x

create_poolset $POOLSET_OUT `
	20M:$DIR\testfile00:x `
	20M:$DIR\testfile01:x `
	r `
	20M:$DIR\testfile01:x `
	20M:$DIR\testfile21:x

# Log poolset structures
echo poolset_in >> $LOG_TEMP
cat $POOLSET_IN >> $LOG_TEMP
echo "" >> $LOG_TEMP
echo poolset_out >> $LOG_TEMP
cat $POOLSET_OUT >> $LOG_TEMP
echo "" >> $LOG_TEMP

# Create a pool
expect_normal_exit $PMEMPOOL create --layout=$LAYOUT obj $POOLSET_IN
cat $LOG >> $LOG_TEMP

# Try to transform poolset
expect_abnormal_exit $PMEMPOOL transform `
    $POOLSET_IN $POOLSET_OUT >> $LOG_TEMP 2>$null

# Check metadata by pmempool info
dump_pool_info $POOLSET_IN >> $LOG_TEMP

mv -Force $LOG_TEMP $LOG
check

pass
