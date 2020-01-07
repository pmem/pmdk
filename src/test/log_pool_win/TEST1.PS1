# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation
#
# src/test/log_pool_win/TEST1 -- unit test for pmemlog_create
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type "any"

setup

create_holey_file 40M $DIR\testfile

#
# TEST1 existing file, file length >= min required size, poolsize == 0
#
expect_normal_exit $Env:EXE_DIR\log_pool_win$Env:EXESUFFIX c $DIR\testfile 0 0600

check_files $DIR\testfile

check

pass
