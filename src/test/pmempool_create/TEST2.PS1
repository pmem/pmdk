# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2018, Intel Corporation
#
#
# pmempool_create/TEST2 -- test for creating pools of specified size
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$POOL="$DIR\file.pool"
$SIZE=(1024 + 512) * 1024 * 1024

require_free_space "${SIZE}b"

# PMEMBLK
expect_normal_exit $PMEMPOOL create -s $SIZE blk 512 $POOL
check_file $POOL
check_signature PMEMBLK $POOL
check_size $SIZE $POOL

# PMEMLOG
rm $POOL
expect_normal_exit $PMEMPOOL create -s $SIZE log $POOL
check_file $POOL
check_signature PMEMLOG $POOL
check_size $SIZE $POOL

#PMEMOBJ
rm $POOL
expect_normal_exit $PMEMPOOL create -s $SIZE obj $POOL
check_file $POOL
check_signature PMEMOBJ $POOL
check_layout "" $POOL
check_size $SIZE $POOL

pass
