# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019, Intel Corporation
#
#
# libpmempool_bttdev/TEST10 -- test for checking btt
#

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type any

setup

$POOL="$DIR\file.pool"
$LOG="out${Env:UNITTEST_NUM}.log"
$LOG_TEMP="out${Env:UNITTEST_NUM}_part.log"
rm $LOG -Force -ea si
rm $LOG_TEMP -Force -ea si
touch $LOG_TEMP

$EXE="$Env:EXE_DIR\libpmempool_test$Env:EXESUFFIX"

expect_normal_exit $BTTCREATE $POOL >> $LOG

expect_normal_exit $PMEMSPOIL $POOL "bttdevice.arena(0).btt_info.checksum=777" `
	"bttdevice.arena(0).btt_info.sig=ERROR" `
	"bttdevice.arena(0).btt_info_backup.checksum=777"

expect_normal_exit $EXE -r 1 -t btt $POOL >> $LOG_TEMP

cat -Encoding Ascii $LOG | out-file -append -Encoding Ascii -literalpath $LOG_TEMP

check_file $POOL

check

pass
