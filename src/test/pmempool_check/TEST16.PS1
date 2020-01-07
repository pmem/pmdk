# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation
#
#
# pmempool_check/TEST16 -- test for checking and fixing pool files crtime
#	invalid links
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$POOLSET="$DIR\pool.set"
$POOL_P1="$DIR\pool.p1"
$POOL_P2="$DIR\pool.p2"
$LOG="out$Env:UNITTEST_NUM.log"
rm $LOG -Force -ea si

create_poolset $POOLSET 20M:$POOL_P1 20M:$POOL_P2

expect_normal_exit $PMEMPOOL create log $POOLSET

[int]$FUTURE_TIME=[int64](([datetime]::UtcNow)-(Get-Date "1/1/1970")).TotalSeconds
$FUTURE_TIME+=60*60
&$PMEMSPOIL -v $POOL_P1 "pool_hdr.crtime=$FUTURE_TIME" >> $LOG

[DateTime]$past=Get-Date
$past=$past.AddHours(-1)
$file=Get-Item $POOL_P1
$file.LastWriteTime=$past

expect_abnormal_exit $PMEMPOOL check -v $POOLSET >> $LOG
expect_abnormal_exit $PMEMPOOL check -vry $POOLSET >> $LOG

# To achieve different checksum, file creation time and file modification time,
# have to be different. The precision of file modification time equals 1 second
# on Windows OS. File modification time is updated during execution
# 'pmempool check' command. That is why the test waits for 1 second.
Start-Sleep -s 1
expect_normal_exit $PMEMPOOL check -vrya $POOLSET >> $LOG
expect_normal_exit $PMEMPOOL info $POOLSET >> $null

check

pass
