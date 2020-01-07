# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2018, Intel Corporation
#
#
# pmempool_info/TEST8 -- test for info command
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$POOL="$DIR\file.pool"
$LOG="out$Env:UNITTEST_NUM.log"
rm $LOG -Force -ea si

expect_normal_exit $PMEMPOOL create blk 512 $POOL
expect_normal_exit $PMEMWRITE $POOL 1:e 2:w:TEST

$INFO_NLBA=(expect_normal_exit $PMEMPOOL info $POOL | sls -Pattern  'External LBA count').ToString().Split(':')[1]
$NLBA=(expect_normal_exit $PMEMPOOL info -m $POOL  | sls -Pattern '\d+: 0x\d+.*' | measure).Count
$NZER=(expect_normal_exit $PMEMPOOL info -mue $POOL | sls -Pattern '\d+: 0x\d+.*' | measure).Count
$NERR=(expect_normal_exit $PMEMPOOL info -muz $POOL | sls -Pattern '\d+: 0x\d+.*' | measure).Count
$NNON=(expect_normal_exit $PMEMPOOL info -mez $POOL | sls -Pattern '\d+: 0x\d+.*' | measure).Count

echo "Number of LBAs: $INFO_NLBA" >> $LOG
echo "Number of LBAs in map: $NLBA" >> $LOG
echo "Number of LBAs [zero]: $NZER" >> $LOG
echo "Number of LBAs [error]: $NERR" >> $LOG
echo "Number of LBAs [no flag]: $NNON" >> $LOG

$NSUM=$NZER + $NERR + $NNON
if ($NSUM -ne $NLBA ) {
    echo "$NSUM != $NLBA"
    fail 1
}
if ($NSUM -ne $INFO_NLBA ) {
    echo "$NSUM != $INFO_NLBA"
    fail 1
}
expect_normal_exit $PMEMPOOL info -dz $POOL >> $LOG

check

pass
