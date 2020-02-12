# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2019, Intel Corporation

#
# src/test/util_ctl/TEST0 -- unit test for util_ctl interface
#

. ..\unittest\unittest.ps1

require_test_type short

setup

expect_normal_exit $Env:EXE_DIR\util_ctl$Env:EXESUFFIX $DIR\testconfig

pass
