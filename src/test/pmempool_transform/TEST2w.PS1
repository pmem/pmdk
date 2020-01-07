# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2018, Intel Corporation
#
#
# pmempool_transform/TEST2 -- test for checking pmempool transform in dry-run
#                             mode; pmem/issues#250
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
    20M:$DIR\testfile0:x `
    20M:$DIR\testfile1:x

create_poolset $POOLSET_OUT `
    20M:$DIR\testfile0:x `
    20M:$DIR\testfile1:x `
    R `
    20M:$DIR\testfile2:x `
    20M:$DIR\testfile3:x

# CLI script for writing some data hitting all the parts
$WRITE_SCRIPT = "$DIR\write_data"
echo @"
pr 22M
srcp 0 TestOK111
srcp 20M TestOK222
"@ | out-file -encoding ASCII -literalpath $WRITE_SCRIPT

# CLI script for reading 9 characters from all the parts
$READ_SCRIPT = "$DIR\read_data"
echo @"
srpr 0 9
srpr 20M 9
"@ | out-file -encoding ASCII -literalpath $READ_SCRIPT

# Create a pool
expect_normal_exit $PMEMPOOL create --layout=$LAYOUT obj $POOLSET_IN
cat $LOG >> $LOG_TEMP

# Write some data into the pool, hitting two part files
expect_normal_exit $PMEMOBJCLI -s $WRITE_SCRIPT $POOLSET_IN >> $LOG_TEMP

# Check if correctly written
expect_normal_exit $PMEMOBJCLI -s $READ_SCRIPT $POOLSET_IN >> $LOG_TEMP

# Run transform in dry-run mode
expect_normal_exit $PMEMPOOL transform -d $POOLSET_IN $POOLSET_OUT >> $LOG
cat $LOG >> $LOG_TEMP

# Check if stored data can be read
expect_normal_exit $PMEMOBJCLI -s $READ_SCRIPT $POOLSET_IN >> $LOG_TEMP

# Check metadata by pmempool info
dump_pool_info $POOLSET_IN >> $LOG_TEMP
dump_pool_info $DIR\testfile0 >> $LOG_TEMP
dump_pool_info $DIR\testfile1 >> $LOG_TEMP

# Make sure no other parts were created
check_no_files $DIR\testfile2 $DIR\testfile3

mv -Force $LOG_TEMP $LOG
check

pass
