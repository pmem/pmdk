# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation
#
# src/test/blk_non_zero/TEST6 -- unit test for
# pmemblk_read/write/set_zero/set_error
#

. ..\unittest\unittest.ps1

require_test_type medium

# doesn't make sense to run in local directory
require_fs_type pmem non-pmem

setup

# single arena write case
$FILE_SIZE = $((1024*1024*1024))
require_free_space "${FILE_SIZE}b"

expect_normal_exit $Env:EXE_DIR\blk_non_zero$Env:EXESUFFIX 512 `
$DIR\testfile1 c $FILE_SIZE	w:0 r:1 r:0 w:1 r:0 r:1 r:2

check

pass
