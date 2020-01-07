# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019, Intel Corporation

#
# src/test/obj_ctl_arenas/TEST4 -- unit test for POBJ_ARENA_ID flag
#

. ..\unittest\unittest.ps1

require_test_type short
require_fs_type any

setup

expect_abnormal_exit $Env:EXE_DIR\obj_ctl_arenas$Env:EXESUFFIX $DIR\testset1 q 2>$null

pass
