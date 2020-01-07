# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019, Intel Corporation
#
#
# libpmempool_bttdev/TEST2 -- test for checking btt
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

expect_normal_exit $PMEMSPOIL $POOL "bttdevice.arena(0).btt_info_backup.sig=ERROR"

expect_normal_exit $EXE -r 1 -t btt $POOL >> $LOG_TEMP

cat -Encoding Ascii $LOG | out-file -append -Encoding Ascii -literalpath $LOG_TEMP

&$PMEMSPOIL $POOL "bttdevice.arena(0).btt_info.sig=ERROR"

expect_normal_exit $EXE -r 1 -t btt $POOL >> $LOG_TEMP

cat -Encoding Ascii $LOG | out-file -append -Encoding Ascii -literalpath $LOG_TEMP

mv -Force $LOG_TEMP $LOG

check_file $POOL

check

pass
