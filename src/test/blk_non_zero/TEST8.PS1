# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation
#
# src/test/blk_non_zero/TEST8 -- unit test for
# pmemblk_read/write/set_zero/set_error
#

. ..\unittest\unittest.ps1

require_test_type medium

# doesn't make sense to run in local directory
require_fs_type pmem non-pmem

setup

# mix writes with set_zero and set_error and check results
$FILE_SIZE = $((1024*1024*1024))
require_free_space "${FILE_SIZE}b"

expect_normal_exit $Env:EXE_DIR\blk_non_zero$Env:EXESUFFIX 512 `
$DIR\testfile1 c $FILE_SIZE e:1 r:1 e:2 w:2 r:2 z:3 e:3 r:3 `
e:4 z:4 r:4 w:5 e:5 z:5 r:5 w:6 z:6 e:6 r:6

check

pass
