# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2018, Intel Corporation
#
#
# pmempool_check/TEST2 -- test for checking pools
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$POOL="$DIR\file.pool"
$POOL_BACKUP="$DIR\file.pool.backup"
$LOG="out$Env:UNITTEST_NUM.log"

rm $LOG -Force -ea si

expect_normal_exit $PMEMPOOL create blk 512 $POOL
check_file $POOL
&$PMEMSPOIL $POOL pool_hdr.signature=ERROR
expect_abnormal_exit $PMEMPOOL check -b $POOL_BACKUP -r $POOL 2>$1 >$null
check_file ${POOL_BACKUP}

pass
