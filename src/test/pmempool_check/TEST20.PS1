# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019, Intel Corporation
#
#
# pmempool_check/TEST20 -- test for checking pools for bad blocks
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$POOL="$DIR\file.pool"
$LOG="out$Env:UNITTEST_NUM.log"
rm $LOG -Force -ea si

echo "PMEMLOG: pmemlog" >> $LOG
rm $POOL -Force -ea si
expect_normal_exit $PMEMPOOL create log $POOL
check_file $POOL
&$PMEMSPOIL -v $POOL "pool_hdr.features.compat=0x01" "pool_hdr.f:checksum_gen" >> $LOG
expect_abnormal_exit $PMEMPOOL check -vry $POOL >> $LOG
&$PMEMSPOIL -v $POOL "pool_hdr.features.compat=0x00" "pool_hdr.f:checksum_gen" >> $LOG
expect_normal_exit $PMEMPOOL check -vry $POOL >> $LOG

echo "PMEMBLK: pmemblk" >> $LOG
rm $POOL -Force -ea si
expect_normal_exit $PMEMPOOL create -w blk 512 $POOL
check_file $POOL
&$PMEMSPOIL -v $POOL "pool_hdr.features.compat=0x01" "pool_hdr.f:checksum_gen" >> $LOG
expect_abnormal_exit $PMEMPOOL check -vry $POOL >> $LOG
&$PMEMSPOIL -v $POOL "pool_hdr.features.compat=0x00" "pool_hdr.f:checksum_gen" >> $LOG
expect_normal_exit $PMEMPOOL check -vry $POOL >> $LOG

echo "PMEMLOG: pmemobj" >> $LOG
rm $POOL -Force -ea si
expect_normal_exit $PMEMPOOL create obj $POOL
check_file $POOL
&$PMEMSPOIL -v $POOL "pool_hdr.features.compat=0x01" "pool_hdr.f:checksum_gen" >> $LOG
expect_abnormal_exit $PMEMPOOL check -vry $POOL >> $LOG
&$PMEMSPOIL -v $POOL "pool_hdr.features.compat=0x00" "pool_hdr.f:checksum_gen" >> $LOG
expect_normal_exit $PMEMPOOL check -vry $POOL >> $LOG

check

pass
