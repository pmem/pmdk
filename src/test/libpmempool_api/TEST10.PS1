# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2018, Intel Corporation
#
#
# libpmempool_api/TEST10 -- test for checking API
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$POOL="$DIR\file.pool"
$LOG="out${Env:UNITTEST_NUM}.log"
$LOG_TEMP="out${Env:UNITTEST_NUM}_part.log"
touch $LOG_TEMP

rm $LOG -Force -ea si
rm $LOG_TEMP -Force -ea si

expect_normal_exit $PMEMPOOL create log $POOL >> $LOG_TEMP
&$PMEMSPOIL $POOL "pool_hdr.major=7" >> $LOG_TEMP
check_file $POOL

expect_normal_exit $Env:EXE_DIR\libpmempool_test$Env:EXESUFFIX `
	-d 0 -r 0 -y 0 $POOL
cat -Encoding Ascii $LOG | out-file -append -encoding ascii -literalpath $LOG_TEMP
rm $POOL -Force

expect_normal_exit $PMEMPOOL create log $POOL >> $LOG_TEMP
&$PMEMSPOIL $POOL "pool_hdr.major=7" >> $LOG_TEMP
check_file $POOL

expect_normal_exit $Env:EXE_DIR\libpmempool_test$Env:EXESUFFIX `
	-d 0 -r 1 -y 1 $POOL
cat -Encoding Ascii $LOG | out-file -append -encoding ascii -literalpath $LOG_TEMP

rm $LOG -Force
mv $LOG_TEMP $LOG

check

pass
