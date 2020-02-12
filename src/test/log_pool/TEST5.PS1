# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation
#
# src/test/log_pool/TEST5 -- unit test for pmemlog_create
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type "any"

setup

#
# TEST5 non-existing file, poolsize > 0
#       (path is invalid, directory does not exist)
#
expect_normal_exit $Env:EXE_DIR\log_pool$Env:EXESUFFIX c \NULL\testfile 20 0600

check_no_files $DIR\testfile

check

pass
