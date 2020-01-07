# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation
#
# src/test/log_pool/TEST20 -- unit test for pmemlog_open
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type "any"

setup

#
# TEST20 non-existing file
#
expect_normal_exit $Env:EXE_DIR\log_pool$Env:EXESUFFIX o $DIR\testfile

check

pass
