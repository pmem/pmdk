# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation
#
# src/test/log_pool/TEST6 -- unit test for pmemlog_create
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type "any"

setup

create_nonzeroed_file 2M 0K $DIR\testfile

#
# TEST6 existing file, file length >= min required size, poolsize == 0
#       (file contains garbage)
#
expect_normal_exit $Env:EXE_DIR\log_pool$Env:EXESUFFIX c $DIR\testfile 0 0600

check_files $DIR\testfile

check

pass
