# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation
#
# src/test/log_pool/TEST4 -- unit test for pmemlog_createte
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type "any"

setup

touch $DIR\testfile

#
# TEST4 existing file, file length < min required size, poolsize == 0
#
expect_normal_exit $Env:EXE_DIR\log_pool$Env:EXESUFFIX c $DIR\testfile 0 0600

check_files $DIR\testfile

check

pass
