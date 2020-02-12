# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# src/test/obj_lane/TEST0 -- single thread unit test for lanes
#

. ..\unittest\unittest.ps1

require_test_type medium

setup

expect_normal_exit $Env:EXE_DIR\obj_lane$Env:EXESUFFIX s 2>$null

check

pass
