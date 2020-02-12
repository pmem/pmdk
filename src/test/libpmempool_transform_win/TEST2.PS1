# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation
#
#
# libpmempool_transform_win/TEST2.PS1 -- test for checking pmempool transform;
#                                    check if flags are supported
#                                    pmem/issues#367
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
	10M:$DIR\part00:x

create_poolset $POOLSET_OUT `
	10M:$DIR\part00:x `
	r `
	10M:$DIR\part10:x

# Create a pool
expect_normal_exit $PMEMPOOL create --layout=$LAYOUT obj $POOLSET_IN
cat -Encoding Ascii $LOG | out-file -append -encoding ascii -literalpath $LOG_TEMP

# Try to transform poolset
$FLAGS = "1" # invalid flag
expect_normal_exit $Env:EXE_DIR\libpmempool_transform_win$Env:EXESUFFIX `
    $POOLSET_IN $POOLSET_OUT $FLAGS
cat -Encoding Ascii $LOG | out-file -append -encoding ascii -literalpath $LOG_TEMP

$FLAGS = "1024"
expect_normal_exit $Env:EXE_DIR\libpmempool_transform_win$Env:EXESUFFIX `
    $POOLSET_IN $POOLSET_OUT $FLAGS
cat -Encoding Ascii $LOG | out-file -append -encoding ascii -literalpath $LOG_TEMP

mv -Force $LOG_TEMP $LOG
check
pass
