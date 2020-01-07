# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation

#
# src/test/obj_toid/TEST0 -- unit test for obj_toid
#

. ..\unittest\unittest.ps1

require_test_type medium

setup

expect_normal_exit $Env:EXE_DIR\obj_toid$Env:EXESUFFIX $DIR\testfile1

check

pass
