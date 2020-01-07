# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2018, Intel Corporation
#
#
# pmempool_create/TEST0 -- test for pmempool create command with default pool size
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$POOL="$DIR\file.pool"

# PMEMBLK
expect_normal_exit $PMEMPOOL create blk 512 $POOL
check_file $POOL
check_signature PMEMBLK $POOL

# PMEMLOG
rm $POOL
expect_normal_exit $PMEMPOOL create log $POOL
check_file $POOL
check_signature PMEMLOG $POOL

# PMEMOBJ
rm $POOL
expect_normal_exit $PMEMPOOL create --layout=pmempool$Env:SUFFIX obj $POOL
check_file $POOL
check_signature PMEMOBJ $POOL
check_layout pmempool$Env:SUFFIX $POOL

pass
