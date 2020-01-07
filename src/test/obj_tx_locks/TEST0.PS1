# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation

#
# src/test/obj_tx_locks/TEST0 -- unit test for transaction locks
#

. ..\unittest\unittest.ps1

require_test_type medium

setup

expect_normal_exit $Env:EXE_DIR\obj_tx_locks$Env:EXESUFFIX $DIR\testfile1

pass
