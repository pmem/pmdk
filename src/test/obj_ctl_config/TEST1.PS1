# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2019, Intel Corporation

#
# src/test/obj_ctl_config/TEST1 -- unit test for ctl configuration
#
$Env:UNITTEST_NAME = "obj_ctl_config/TEST1"
$Env:UNITTEST_NUM = "100" # TEST0 poses as different test numbers

. ..\unittest\unittest.ps1

require_test_type short

setup

expect_normal_exit $PMEMPOOL create --layout obj_ctl_config obj $DIR\testfile

$Env:PMEMOBJ_CONF_FILE = ""
expect_normal_exit $Env:EXE_DIR\obj_ctl_config$Env:EXESUFFIX $DIR\testfile

pass
