# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# src/test/obj_pmalloc_oom_mt/TEST0 -- unit test for pmalloc interface
#

. ..\unittest\unittest.ps1

require_test_type medium

setup

$Env:PMEM_IS_PMEM_FORCE=1

expect_normal_exit $Env:EXE_DIR\obj_pmalloc_oom_mt$Env:EXESUFFIX $DIR\testfile

pass
