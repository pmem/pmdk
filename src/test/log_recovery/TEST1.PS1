# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2019, Intel Corporation

#
# src/test/log_recovery/TEST0 -- unit test for pmemlog recovery
#                                pmemlog_appendv() is used to append data
#

. ..\unittest\unittest.ps1

require_test_type medium

require_build_type nondebug static-nondebug

setup

touch $DIR\testfile1

expect_normal_exit $Env:EXE_DIR\log_recovery$Env:EXESUFFIX $DIR\testfile1 v

check_pool $DIR\testfile1

check

pass
