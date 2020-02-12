# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2018, Intel Corporation
#
#
# libpmempool_map_flog/TEST3 -- test for checking map and flog
#

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type any

setup

$POOL = "$DIR\file.pool"
$LOG = "out${Env:UNITTEST_NUM}.log"
$LOG_TEMP = "out${Env:UNITTEST_NUM}_part.log"
rm $LOG -Force -ea si
touch $LOG
rm $LOG_TEMP -Force -ea si
touch $LOG_TEMP

$EXE = "$Env:EXE_DIR\libpmempool_test$Env:EXESUFFIX"

$ent_val = 10

expect_normal_exit $BTTCREATE $POOL

for ($i=0; $i -lt $ent_val; $i++) {
    $spcmd = "bttdevice.arena.btt_map($i)=0xC000000${i}"
    echo $spcmd | out-file -append -encoding ascii -literalpath $LOG_TEMP
    &$PMEMSPOIL $POOL $spcmd
}

$spcmd = "bttdevice.arena.btt_map(5)=0xC0000002"
echo $spcmd | out-file -append -encoding ascii -literalpath $LOG_TEMP
&$PMEMSPOIL $POOL $spcmd

$spcmd = "bttdevice.arena.btt_map(6)=0xC0000002"
echo $spcmd | out-file -append -encoding ascii -literalpath $LOG_TEMP
&$PMEMSPOIL $POOL $spcmd

$spcmd = "bttdevice.arena.btt_map(7)=0xC0000002"
echo $spcmd | out-file -append -encoding ascii -literalpath $LOG_TEMP
&$PMEMSPOIL $POOL $spcmd

$spcmd = "bttdevice.arena.btt_map(10)=0xC0000002"
echo $spcmd | out-file -append -encoding ascii -literalpath $LOG_TEMP
&$PMEMSPOIL $POOL $spcmd

$spcmd = "bttdevice.arena.btt_map(11)=0xC0000002"
echo $spcmd | out-file -append -encoding ascii -literalpath $LOG_TEMP
&$PMEMSPOIL $POOL $spcmd

$spcmd = "bttdevice.arena.btt_map(12)=0xC0000003"
echo $spcmd | out-file -append -encoding ascii -literalpath $LOG_TEMP
&$PMEMSPOIL $POOL $spcmd

expect_normal_exit $EXE -r 1 -t btt -a 1 $POOL
cat -Encoding Ascii $LOG | out-file -append -encoding ascii -literalpath $LOG_TEMP

expect_normal_exit $PMEMPOOL info $POOL -f btt -m | `
    Select-String -encoding ASCII -Pattern "error" | %{$_.Line} | out-file -append -encoding ascii -literalpath $LOG_TEMP

mv -Force $LOG_TEMP $LOG
check_file $POOL

check
pass
