# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2018, Intel Corporation
#
#
# pmempool_create/TEST4 -- test for creating pool with specified permissions
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$POOL="$DIR\file.pool"
# PMEMBLK
expect_normal_exit $PMEMPOOL create -m 0600 blk 512 $POOL
check_file $POOL
check_signature PMEMBLK $POOL
check_mode 0600 $POOL

# PMEMLOG
rm -Force $POOL
expect_normal_exit $PMEMPOOL create -m 0500 log $POOL
check_file $POOL
check_signature PMEMLOG $POOL
check_mode 0500 $POOL

# PMEMOBJ
rm -Force $POOL
expect_normal_exit $PMEMPOOL create -m 0500 --layout=pmempool$Env:SUFFIX obj $POOL
check_file $POOL
check_signature PMEMOBJ $POOL
check_layout pmempool$Env:SUFFIX $POOL
check_mode 0500 $POOL

pass
