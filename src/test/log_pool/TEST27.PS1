# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation
#
# src/test/log_pool/TEST27 -- unit test for pmemlog_open
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type "any"

setup

#
# TEST27 existing file, file size >= min required size
#
create_poolset $DIR\testset 20M:$DIR\testfile1:x 20M:$DIR\testfile2:x
expect_normal_exit $Env:EXE_DIR\log_pool$Env:EXESUFFIX c $DIR\testset 0 0600

expect_normal_exit $Env:EXE_DIR\log_pool$Env:EXESUFFIX o $DIR\testset

check

pass
