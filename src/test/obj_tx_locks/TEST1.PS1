# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation

#
# src/test/obj_tx_locks/TEST1 -- unit test for transaction locks
#

. ..\unittest\unittest.ps1

require_test_type medium

$Env:PMEM_LOG_LEVEL = ""
$Env:PMEM_LOG_FILE = ""
$Env:PMEMOBJ_LOG_LEVEL = ""
$Env:PMEMOBJ_LOG_FILE = ""

require_fs_type any
require_build_type debug nondebug

setup

expect_normal_exit $Env:EXE_DIR\obj_tx_locks$Env:EXESUFFIX $DIR\testfile1 m

pass
