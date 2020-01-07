# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation
#
# src/test/log_pool_lock/TEST0 -- unit test for pmemlog_open
#

. ..\unittest\unittest.ps1

require_test_type medium
require_no_superuser

# doesn't make sense to run in local directory
require_fs_type any

setup

expect_normal_exit $Env:EXE_DIR\log_pool_lock$Env:EXESUFFIX $DIR\testfile1

pass
