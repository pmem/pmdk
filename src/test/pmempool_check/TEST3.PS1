# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2018, Intel Corporation
#
#
# pmempool_check/TEST3 -- test for checking pools
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$POOL="$DIR\file.pool"
$LOG="out$Env:UNITTEST_NUM.log"

rm $LOG -Force -ea si
expect_normal_exit $PMEMPOOL create -w blk 512 $POOL
check_file $POOL
& $PMEMSPOIL $POOL pool_hdr.signature=ERROR "pmemblk.arena.btt_map(0)=0xc0000001" "pmemblk.arena.btt_flog(0).seq=5"

expect_normal_exit $PMEMPOOL check -avry $POOL >> $LOG

check

pass
