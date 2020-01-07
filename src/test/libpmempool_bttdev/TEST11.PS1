# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019, Intel Corporation
#
#
# libpmempool_bttdev/TEST11 -- test for checking btt
#

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type any
#
# For a large pool, util_map consumes so much memory.
#
require_free_physical_memory 3G
#
# Automatic page file size management eliminate the risk
# of exceeding the available resources.
#
require_automatic_managed_pagefile

setup

$POOL="$DIR\file.pool"
$LOG="out${Env:UNITTEST_NUM}.log"
$LOG_TEMP="out${Env:UNITTEST_NUM}_part.log"
rm $LOG -Force -ea si
rm $LOG_TEMP -Force -ea si
touch $LOG_TEMP

$EXE="$Env:EXE_DIR\libpmempool_test$Env:EXESUFFIX"

truncate -s 1T $POOL
expect_normal_exit $BTTCREATE -s 1T -b 512M -t $POOL >> $LOG

expect_normal_exit $PMEMSPOIL $POOL "bttdevice.arena(1).btt_info.checksum=777" `
	"bttdevice.arena(1).btt_info.sig=ERROR" `
	"bttdevice.arena(1).btt_info_backup.checksum=777"

expect_normal_exit $EXE -r 1 -t btt $POOL >> $LOG_TEMP

cat -Encoding Ascii $LOG | out-file -append -Encoding Ascii -literalpath $LOG_TEMP

check_file $POOL

check

pass
