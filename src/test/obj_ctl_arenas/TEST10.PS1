# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation

#
# src/test/obj_ctl_arenas/TEST10 -- test for max default arenas
#

. ..\unittest\unittest.ps1

require_test_type short
require_fs_type any

setup

expect_normal_exit $Env:EXE_DIR\obj_ctl_arenas$Env:EXESUFFIX $DIR\testset1 b

pass
