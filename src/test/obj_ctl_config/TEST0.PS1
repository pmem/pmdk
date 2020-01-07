# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2019, Intel Corporation

#
# src/test/obj_ctl_config/TEST0 -- unit test for ctl configuration
#

. ..\unittest\unittest.ps1

require_test_type short

setup

expect_normal_exit $PMEMPOOL create --layout obj_ctl_config obj $DIR\testfile

$CONFIG_ID = 0

Do {
	$Env:UNITTEST_NUM = $CONFIG_ID
	$CONFIG_FILE = "config" + $CONFIG_ID

	if (!(Test-Path $CONFIG_FILE -PathType Leaf)) {
		break
	}

	if (!(Get-Content $CONFIG_FILE | Select-String "#" -quiet)) {
		$Env:PMEMOBJ_CONF = Get-Content $CONFIG_FILE -Raw
		expect_normal_exit $Env:EXE_DIR\obj_ctl_config$Env:EXESUFFIX $DIR\testfile
		$Env:PMEMOBJ_CONF = ""
		check
	}

	$Env:PMEMOBJ_CONF_FILE = $CONFIG_FILE
	expect_normal_exit $Env:EXE_DIR\obj_ctl_config$Env:EXESUFFIX $DIR\testfile
	$Env:PMEMOBJ_CONF_FILE = ""
	check

	$CONFIG_ID = $CONFIG_ID + 1
} While ($true)

pass
