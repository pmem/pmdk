# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2018, Intel Corporation
#
#
# pmempool_create/TEST5 -- test for pmempool create for checking bsize argument
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$POOL="$DIR\file.pool"
$POOL2="$DIR\file2.pool"

# PMEMBLK
expect_abnormal_exit $PMEMPOOL create blk $POOL 2> err5w.log

expect_abnormal_exit $PMEMPOOL create blk 4294967296 $POOL2 2>> err5w.log

# XXX: checking output is not possible as powershell's error message layout
# depends on terminal size.

# check
pass
