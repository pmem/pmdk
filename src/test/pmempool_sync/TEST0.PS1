# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2018, Intel Corporation
#
#
# pmempool_sync/TEST0 -- test for checking pmempool sync
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
$M = 1024 * 1024
$M20 = 20 * $M
$M40 = 40 * $M
$MMAP_ALIGN = "65536"
$ADDR_MASK = "0xFFFF0000"

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

# Create poolset
expect_normal_exit $PMEMPOOL create --layout=$LAYOUT obj $POOLSET
cat $LOG >> $LOG_TEMP

# Write some data into the pool, hitting three part files
expect_normal_exit $PMEMOBJCLI -s $WRITE_SCRIPT $POOLSET | out-file -encoding ASCII -Append -literalpath $LOG_TEMP

# Check if correctly written
expect_normal_exit $PMEMOBJCLI -s $READ_SCRIPT $POOLSET | out-file -encoding ASCII -Append -literalpath  $LOG_TEMP

# Find root offset
$TMP_FILE = "$DIR\obj_info"
expect_normal_exit $PMEMPOOL info -f obj -o $DIR\testfile1 | out-file -encoding ASCII -literalpath  $TMP_FILE

$ROOT_ADDR = (Get-Content $TMP_FILE | Select-String -pattern "Root offset").ToString().Split(':')[1]

# Calculate offsets from the root object start to hit each part file
$PART1_FILE_SIZE = (Get-Item "$DIR\testfile1").length
$PART1_SIZE = ($PART1_FILE_SIZE -band $ADDR_MASK) - $ROOT_ADDR
$PART2_OFFSET = $M20 - $PART1_SIZE + $MMAP_ALIGN

$PART2_FILE_SIZE = (Get-Item "$DIR\testfile2").length
$PART2_SIZE = ($PART2_FILE_SIZE -band $ADDR_MASK) - $MMAP_ALIGN
$PART3_OFFSET = $M40 - ($PART1_SIZE + $PART2_SIZE) + $MMAP_ALIGN

# Corrupt data in primary replica
expect_normal_exit $DDMAP -o "$DIR\testfile1" -s $ROOT_ADDR -d "Wrong1234"
expect_normal_exit $DDMAP -o "$DIR\testfile2" -s $PART2_OFFSET -d "Wrong5678"
expect_normal_exit $DDMAP -o "$DIR\testfile3" -s $PART3_OFFSET -d "Wrong9ABC"

# Check if corrupted
expect_normal_exit $PMEMOBJCLI -s $READ_SCRIPT $POOLSET | out-file -encoding ASCII -Append -literalpath $LOG_TEMP

# Corrupt metadata in primary replica
expect_normal_exit $PMEMSPOIL $DIR\testfile1 "pool_hdr.uuid=0000000000000000" `
    >> $LOG_TEMP

# Synchronize replicas
expect_normal_exit $PMEMPOOL sync $POOLSET >> $LOG_TEMP

# Check if correctly copied
expect_normal_exit $PMEMOBJCLI -s $READ_SCRIPT $POOLSET | out-file -encoding ASCII -Append -literalpath $LOG_TEMP

mv -Force $LOG_TEMP $LOG
check

pass
