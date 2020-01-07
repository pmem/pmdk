# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation
#
# src/test/log_basic/TEST6 -- unit test for:
# - pmemlog_nbyte
# - pmemlog_check
# - pmemlog_open
# - pmemlog_close

. ..\unittest\unittest.ps1

require_test_type medium

# doesn't make sense to run in local directory
require_fs_type any

setup

create_poolset $DIR\testset1 2M:$DIR\testfile1:x 2M:$DIR\testfile2:x

expect_normal_exit $Env:EXE_DIR\log_basic$Env:EXESUFFIX $DIR\testset1 c n l h

check_pool $DIR\testfile1

check

pass
