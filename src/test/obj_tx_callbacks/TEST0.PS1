# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation

#
# src/test/obj_tx_callbacks/TEST0 -- unit test for transaction stage callbacks
#

. ..\unittest\unittest.ps1

require_test_type medium

setup

expect_normal_exit $ENV:EXE_DIR\obj_tx_callbacks$Env:EXESUFFIX $DIR\testfile1

check

pass
