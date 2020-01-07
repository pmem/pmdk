# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019, Intel Corporation

#
# src/test/obj_ctl_stats/TEST0 -- unit test for the libpmemobj statistics module
#

. ..\unittest\unittest.ps1

require_test_type short
require_fs_type any

setup

expect_normal_exit $Env:EXE_DIR\obj_ctl_stats$Env:EXESUFFIX $DIR\testfile1

pass
