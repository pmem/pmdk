# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2019, Intel Corporation

. ..\unittest\unittest.ps1

require_test_type medium

setup

$Env:PMEM_IS_PMEM_FORCE=1
$Env:PMEMOBJ_LOG_LEVEL=1

expect_normal_exit $Env:EXE_DIR\obj_action$Env:EXESUFFIX $DIR\testfile1

pass
