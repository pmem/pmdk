# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation
#
# src/test/obj_check/TEST1 -- unit test for pmemobj_check
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

expect_normal_exit $PMEMPOOL create obj $DIR\testfile

& $PMEMSPOIL $DIR\testfile pool_hdr.signature=ERROR
expect_normal_exit $Env:EXE_DIR\obj_check$Env:EXESUFFIX $DIR\testfile

check

pass
