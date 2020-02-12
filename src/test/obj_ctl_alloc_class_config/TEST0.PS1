# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019, Intel Corporation

. ..\unittest\unittest.ps1

require_test_type short
require_fs_type any

setup

$Env:PMEMOBJ_CONF_FILE="alloc_class_config"
expect_normal_exit $Env:EXE_DIR\obj_ctl_alloc_class_config$Env:EXESUFFIX $DIR\testfile

check

pass
