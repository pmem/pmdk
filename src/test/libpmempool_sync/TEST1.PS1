# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation
#
#
# libpmempool_sync/TEST1.PS1 -- test for checking replica sync
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
    20M:$DIR\testfile2:x `
    21M:$DIR\testfile3:x `
    R `
    40M:$DIR\testfile4:x `
    20M:$DIR\testfile5:x

# CLI script for writing some data hitting all the parts
$WRITE_SCRIPT = "$DIR\write_data"
echo @"
pr 55M
srcp 0 TestOK111
srcp 20M TestOK222
srcp 40M TestOK333
"@ | out-file -encoding ASCII -literalpath $WRITE_SCRIPT

# CLI script for reading 9 characters from all the parts
$READ_SCRIPT = "$DIR\read_data"
echo @"
srpr 0 9
srpr 20M 9
srpr 40M 9
"@ | out-file -encoding ASCII -literalpath $READ_SCRIPT

# Create a pool
expect_normal_exit $PMEMPOOL create --layout=$LAYOUT obj $POOLSET
cat -Encoding Ascii $LOG | out-file -append -encoding ascii -literalpath $LOG_TEMP

# Write some data into the pool, hitting three part files
expect_normal_exit $PMEMOBJCLI -s $WRITE_SCRIPT $POOLSET | out-file -append -encoding ascii -literalpath $LOG_TEMP

# Check if correctly written
expect_normal_exit $PMEMOBJCLI -s $READ_SCRIPT $POOLSET | out-file -append -encoding ascii -literalpath $LOG_TEMP

# Delete the second part in the primary replica
rm $DIR\testfile2 -Force -ea si

# Synchronize replicas
$FLAGS = "0"
expect_normal_exit $Env:EXE_DIR\libpmempool_sync$Env:EXESUFFIX `
    $POOLSET $FLAGS
cat -Encoding Ascii $LOG | out-file -append -encoding ascii -literalpath $LOG_TEMP

# Check if correctly synchronized
expect_normal_exit $PMEMOBJCLI -s $READ_SCRIPT $POOLSET | out-file -append -encoding ascii -literalpath $LOG_TEMP

mv -Force $LOG_TEMP $LOG
check
pass
