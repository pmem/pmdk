# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation
#
# src/test/log_pool/TEST0 -- unit test for pmemlog_create
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type "any"

setup

#
# TEST0 non-existing file, poolsize > 0
#
expect_normal_exit $Env:EXE_DIR\log_pool$Env:EXESUFFIX c $DIR\testfile 20 0600

check_files $DIR\testfile

check

pass
