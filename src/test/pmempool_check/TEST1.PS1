# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2020, Intel Corporation
#
#
# pmempool_check/TEST1 -- test for checking pools
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$POOL="$DIR\file.pool"
$LOG="out$Env:UNITTEST_NUM.log"

echo "PMEMLOG: pool_hdr" > $LOG
expect_normal_exit $PMEMPOOL create log $POOL
check_file $POOL

Invoke-Expression "$PMEMSPOIL -v $POOL pool_hdr.major=0x0 pool_hdr.features.compat=0xfe pool_hdr.features.incompat=0xfe pool_hdr.features.ro_compat=0xfb pool_hdr.shutdown_state.usc=0 pool_hdr.shutdown_state.uuid=0 'pool_hdr.shutdown_state.f:checksum_gen' pool_hdr.unused=ERROR >> $LOG"
expect_normal_exit $PMEMPOOL check -vry $POOL >> $LOG

echo "PMEMLOG: pmemlog" >> $LOG
rm $POOL -Force
expect_normal_exit $PMEMPOOL create log $POOL
check_file $POOL
Invoke-Expression "$PMEMSPOIL -v $POOL pmemlog.start_offset=0 pmemlog.end_offset=0 >> $LOG"
expect_normal_exit $PMEMPOOL check -vry $POOL >> $LOG

echo "PMEMBLK: pool_hdr" >> $LOG
rm $POOL -Force
expect_normal_exit $PMEMPOOL create -w blk 512 $POOL
check_file $POOL
Invoke-Expression "$PMEMSPOIL -v $POOL pool_hdr.signature=ERROR	pool_hdr.major=0xff	pool_hdr.features.compat=0xfe pool_hdr.features.incompat=0xfb pool_hdr.features.ro_compat=0xff pool_hdr.unused=ERROR >> $LOG"
expect_normal_exit $PMEMPOOL check -vry $POOL >> $LOG

echo "PMEMBLK: pmemblk" >> $LOG
rm $POOL -Force
expect_normal_exit $PMEMPOOL create -w blk 512 $POOL
check_file $POOL
Invoke-Expression "$PMEMSPOIL -v $POOL pool_hdr.signature=ERROR pmemblk.bsize=0 >> $LOG"
expect_normal_exit $PMEMPOOL check -vry $POOL >> $LOG

check

pass
