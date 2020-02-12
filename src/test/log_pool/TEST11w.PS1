# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation
#
# src/test/log_pool/TEST11 -- unit test for pmemlog_create
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type "any"

setup

#
# TEST11 non-existing file, poolsize > 0
#        (pool set)
#
create_poolset $DIR\testset 20M:$DIR\testfile1:x 20M:$DIR\testfile2:x

expect_normal_exit $Env:EXE_DIR\log_pool$Env:EXESUFFIX c $DIR\testset 0 0600

check_files $DIR\testset $DIR\testfile1 $DIR\testfile2

check

pass
