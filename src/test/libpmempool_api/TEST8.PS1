# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2018, Intel Corporation
#
#
# libpmempool_api/TEST8 -- test for checking API
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$POOL="$DIR\file.pool"

expect_normal_exit $PMEMPOOL create log $POOL
check_file $POOL
expect_normal_exit $PMEMWRITE $POOL TEST

expect_normal_exit $Env:EXE_DIR\libpmempool_test$Env:EXESUFFIX -t blk $POOL
check

pass
