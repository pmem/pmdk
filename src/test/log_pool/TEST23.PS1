# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation
#
# src/test/log_pool/TEST23 -- unit test for pmemlog_open
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type "any"

setup

create_holey_file 20M $DIR\testfile

#
# TEST23 existing file, file size >= min required size
#        (empty pool header)
#
expect_normal_exit $Env:EXE_DIR\log_pool$Env:EXESUFFIX o $DIR\testfile

check

pass
