# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2018, Intel Corporation
#
#
# pmempool_sync/TEST4 -- test for checking pmempool sync;
#                        pmem/issues#269
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
create_poolset  $POOLSET `
    20M:$DIR\testfile00:x `
    20M:$DIR\testfile01:x `
    R `
    30M:$DIR\testfile10:x `
    20M:$DIR\testfile11:x

# CLI script for writing some data hitting all the parts
$WRITE_SCRIPT = "$DIR\write_data"
echo @"
pr 30M
srcp 0 TestOK111
srcp 25M TestOK222
"@ | out-file -encoding ASCII -literalpath $WRITE_SCRIPT

# CLI script for reading 9 characters from all the parts
$READ_SCRIPT = "$DIR\read_data"
echo @"
srpr 0 9
srpr 25M 9
"@ | out-file -encoding ASCII -literalpath $READ_SCRIPT

# Create a poolset
expect_normal_exit $PMEMPOOL create --layout=$LAYOUT obj $POOLSET
cat $LOG >> $LOG_TEMP

# Write some data into the pool, hitting all part files
expect_normal_exit $PMEMOBJCLI -s $WRITE_SCRIPT $POOLSET >> $LOG_TEMP

# Check if correctly written
expect_normal_exit $PMEMOBJCLI -s $READ_SCRIPT $POOLSET >> $LOG_TEMP

# Delete the second part in the primary replica
rm $DIR\testfile00 -Force -ea si

# Synchronize replicas
expect_normal_exit $PMEMPOOL sync $POOLSET >> $LOG_TEMP

# Check metadata by pmempool info
dump_pool_info $POOLSET >> $LOG_TEMP
dump_pool_info $DIR\testfile00 >> $LOG_TEMP
dump_pool_info $DIR\testfile01 >> $LOG_TEMP
dump_pool_info $DIR\testfile10 >> $LOG_TEMP
dump_pool_info $DIR\testfile11 >> $LOG_TEMP

mv -Force $LOG_TEMP $LOG
check

pass
