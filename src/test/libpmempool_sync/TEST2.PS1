# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation
#
#
# libpmempool_sync/TEST2.PS1 -- test for checking replica sync
#                               check if flags are supported
#                               pmem/issues#367
#

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type any
require_short_path

setup

$LOG = "out${Env:UNITTEST_NUM}.log"
$LOG_TEMP = "out${Env:UNITTEST_NUM}_part.log"
rm $LOG -Force -ea si
touch $LOG
rm $LOG_TEMP -Force -ea si
touch $LOG_TEMP

$LAYOUT = "OBJ_LAYOUT${Env:SUFFIX}"
$POOLSET = "$DIR\poolset"

# Create poolset file
create_poolset $POOLSET `
	10M:$DIR\part00:x `
	r `
	10M:$DIR\part10:x

# Create a pool
expect_normal_exit $PMEMPOOL create --layout=$LAYOUT obj $POOLSET
cat -Encoding Ascii $LOG | out-file -append -encoding ascii -literalpath $LOG_TEMP

# Delete a part from the second replica
rm $DIR\part10 -Force -ea si

# Try to synchronize replicas
$FLAGS = "32" # invalid flag
expect_normal_exit $Env:EXE_DIR\libpmempool_sync$Env:EXESUFFIX `
	$POOLSET $FLAGS
cat -Encoding Ascii $LOG | out-file -append -encoding ascii -literalpath $LOG_TEMP

$FLAGS = "1024"
expect_normal_exit $Env:EXE_DIR\libpmempool_sync$Env:EXESUFFIX `
	$POOLSET $FLAGS
cat -Encoding Ascii $LOG | out-file -append -encoding ascii -literalpath $LOG_TEMP

mv -Force $LOG_TEMP $LOG
check
pass
