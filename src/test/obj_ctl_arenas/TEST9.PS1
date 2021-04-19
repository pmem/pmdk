# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation

#
# src/test/obj_ctl_arenas/TEST9 -- test for global arena type assignment
#

. ..\unittest\unittest.ps1

require_test_type short
require_fs_type any

setup

$Env:PMEMOBJ_CONF += "heap.arenas_assignment_type=thread"
expect_normal_exit $Env:EXE_DIR\obj_ctl_arenas$Env:EXESUFFIX $DIR\testset1 d

pass
