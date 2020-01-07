# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation
#
#
# libpmempool_transform_win/TEST1.PS1 -- test for checking pmempool transform;
#                                    pmem/issues#320
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
    10M:$DIR\testfile1:x `
    R `
    10M:$DIR\testfile2:x `
    10M:$DIR\testfile3:x `
    10M:$DIR\testfile4:x `

create_poolset $POOLSET_OUT `
    20M:$DIR\testfile0:x `
    10M:$DIR\testfile1:x `
    R `
    10M:$DIR\testfile2:x `
    10M:$DIR\testfile3:x `
    10M:$DIR\testfile4:x `
    R `
    10M:$DIR\testfile5:x `
    20M:$DIR\testfile6:x

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
cat -Encoding Ascii $LOG | out-file -append -encoding ascii -literalpath $LOG_TEMP

# Write some data into the pool, hitting two part files
expect_normal_exit $PMEMOBJCLI -s $WRITE_SCRIPT $POOLSET_IN | out-file -append -encoding ascii -literalpath $LOG_TEMP

# Check if correctly written
expect_normal_exit $PMEMOBJCLI -s $READ_SCRIPT $POOLSET_IN | out-file -append -encoding ascii -literalpath $LOG_TEMP

# Transform poolset
$FLAGS = "0"
expect_normal_exit $Env:EXE_DIR\libpmempool_transform_win$Env:EXESUFFIX `
    $POOLSET_IN $POOLSET_OUT $FLAGS
cat -Encoding Ascii $LOG | out-file -append -encoding ascii -literalpath $LOG_TEMP

# Check if correctly copied
expect_normal_exit $PMEMOBJCLI -s $READ_SCRIPT $POOLSET_OUT | out-file -append -encoding ascii -literalpath $LOG_TEMP

# Check metadata by pmempool info
for ($i=0; $i -lt 7; $i++) {
    dump_pool_info $DIR\testfile$i | out-file -append -encoding ascii -literalpath $LOG_TEMP
}

mv -Force $LOG_TEMP $LOG
check
pass
