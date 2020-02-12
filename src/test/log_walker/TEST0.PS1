# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2019, Intel Corporation

#
# src/test/log_walker/TEST0 -- unit test to verify pool's write-protection
#                              in debug mode
#

. ..\unittest\unittest.ps1

require_test_type medium

require_build_type debug

setup

# this test invokes sigsegvs by design

touch $DIR\testfile1

expect_normal_exit $Env:EXE_DIR\log_walker$Env:EXESUFFIX $DIR\testfile1

check_pool $DIR\testfile1

check

pass
