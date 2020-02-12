# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019, Intel Corporation
#
#
# pmempool_info/TEST19 -- test for info command
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$POOL="$DIR\file.pool"
$LOG="out$Env:UNITTEST_NUM.log"
$ERR="err$Env:UNITTEST_NUM.log"

expect_normal_exit $PMEMPOOL create log $POOL
&$PMEMSPOIL -v $POOL "pool_hdr.features.compat=0x01" "pool_hdr.f:checksum_gen" | out-file -Encoding ascii $LOG
expect_abnormal_exit cmd /c $PMEMPOOL info $POOL `>`> $LOG 2`>$ERR
&$PMEMSPOIL -v $POOL "pool_hdr.features.compat=0x00" "pool_hdr.f:checksum_gen" | out-file -Append -Encoding ascii $LOG
expect_normal_exit $PMEMPOOL info $POOL | out-file -Append -Encoding ascii $LOG

check

pass
