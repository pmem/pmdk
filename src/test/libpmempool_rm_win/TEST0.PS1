# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2018, Intel Corporation
#
#
# libpmempool_rm_win/TEST0 -- test pmempool_rm with single-file pools
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

enable_log_append

expect_normal_exit $PMEMPOOL$EXESUFFIX create blk 512 $DIR\pool.blk
expect_normal_exit $PMEMPOOL$EXESUFFIX create log $DIR\pool.log
expect_normal_exit $PMEMPOOL$EXESUFFIX create obj $DIR\pool.obj
check_files $DIR\pool.blk $DIR\pool.log $DIR\pool.obj

# this should remove all the pool files
expect_normal_exit $Env:EXE_DIR\libpmempool_rm_win$Env:EXESUFFIX $DIR\pool.blk $DIR\pool.log $DIR\pool.obj

# check if all pool files are removed
check_no_files $DIR\pool.blk $DIR\pool.log $DIR\pool.obj

# this should report errors for all files
expect_normal_exit $Env:EXE_DIR\libpmempool_rm_win$Env:EXESUFFIX $DIR\pool.blk $DIR\pool.log $DIR\pool.obj
# this should ignore all errors
expect_normal_exit $Env:EXE_DIR\libpmempool_rm_win$Env:EXESUFFIX -f $DIR\pool.blk $DIR\pool.log $DIR\pool.obj

check

pass
