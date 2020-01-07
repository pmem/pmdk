# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019, Intel Corporation

#
# src/test/obj_ctl_debug/TEST0.PS1 -- unit test (short) for ctl_debug
#

. ..\unittest\unittest.ps1

require_test_type short
require_fs_type any
require_build_type any

setup

expect_normal_exit $Env:EXE_DIR\obj_ctl_debug$Env:EXESUFFIX $DIR\testfile

pass
