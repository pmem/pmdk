# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2018, Intel Corporation
#
#
# pmempool_transform/TEST6 -- test for checking pmempool transform;
#                             pmem/issues#274
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
$POOLSET_OUT1 = "$DIR\poolset.out1"
$POOLSET_OUT2 = "$DIR\poolset.out2"

# Create poolset files
create_poolset $POOLSET_IN `
    20M:$DIR\testfile00:x `
    20M:$DIR\testfile01:x

create_poolset $POOLSET_OUT1 `
    20M:$DIR\testfile00:x `
    20M:$DIR\testfile01:x `
    R `
    20M:$DIR\testfile10:x `
    20M:$DIR\testfile11:x

create_poolset $POOLSET_OUT2 `
    20M:$DIR\testfile00:x `
    20M:$DIR\testfile01:x `
    R `
    20M:$DIR\testfile20:x `
    20M:$DIR\testfile21:x

# Log poolset structures
echo poolset_in >> $LOG_TEMP
cat $POOLSET_IN >> $LOG_TEMP
echo "" >> $LOG_TEMP
echo poolset_out1 >> $LOG_TEMP
cat $POOLSET_OUT1 >> $LOG_TEMP
echo "" >> $LOG_TEMP
echo poolset_out2 >> $LOG_TEMP
cat $POOLSET_OUT2 >> $LOG_TEMP
echo "" >> $LOG_TEMP

# Create a pool
expect_normal_exit $PMEMPOOL create --layout=$LAYOUT obj $POOLSET_IN
cat $LOG >> $LOG_TEMP

# Transform poolset
expect_normal_exit $PMEMPOOL transform $POOLSET_IN $POOLSET_OUT1 >> $LOG_TEMP

# Try to transform poolset again
expect_abnormal_exit $PMEMPOOL transform `
    $POOLSET_IN $POOLSET_OUT2 >> $LOG_TEMP 2>$null

# Check metadata by pmempool info
dump_pool_info $POOLSET_OUT1 >> $LOG_TEMP

mv -Force $LOG_TEMP $LOG
check

pass
