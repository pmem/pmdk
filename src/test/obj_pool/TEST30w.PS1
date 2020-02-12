# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2019, Intel Corporation
#
# src/test/obj_pool/TEST30w -- create a pool with the smallest parts possible
#

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type any

setup

$MIN_PART = (2 * 1024 * 1024).toString() + "B" # 2MiB

create_poolset $DIR\testset ${MIN_PART}:$DIR\testfile1:x ${MIN_PART}:$DIR\testfile2:x `
    ${MIN_PART}:$DIR\testfile3:x ${MIN_PART}:$DIR\testfile4:x ${MIN_PART}:$DIR\testfile5:x

expect_normal_exit $Env:EXE_DIR\obj_pool$Env:EXESUFFIX c $DIR\testset test 0 0600

check

pass
