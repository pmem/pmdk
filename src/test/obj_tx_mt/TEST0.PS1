# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation

#
# src/test/obj_tx_mt/TEST0 -- multi-threaded test for pmemobj_tx*
#

. ..\unittest\unittest.ps1

require_test_type medium

# doesn't make sense to run in local directory
require_fs_type any

setup

expect_normal_exit $Env:EXE_DIR\obj_tx_mt$Env:EXESUFFIX $DIR\testfile1

pass
