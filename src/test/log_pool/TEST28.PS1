# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation
#
# src/test/log_pool/TEST28 -- unit test for pmemlog_open
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type "any"

setup

#
# TEST28 existing file, file size >= min required size
#
expect_normal_exit $Env:EXE_DIR\log_pool$Env:EXESUFFIX c $DIR\testfile1 20 0600

cp $DIR\testfile1 $DIR\testfile2

&$PMEMSPOIL $DIR\testfile1 pool_hdr.uuid=1111111111111111 `
        pool_hdr.next_part_uuid=1111111111111111 `
        pool_hdr.prev_part_uuid=1111111111111111 `
        pool_hdr.next_repl_uuid=2222222222222222 `
        pool_hdr.prev_repl_uuid=2222222222222222 `
        "pool_hdr.f:checksum_gen"

&$PMEMSPOIL $DIR\testfile2 pool_hdr.uuid=2222222222222222 `
        pool_hdr.next_part_uuid=2222222222222222 `
        pool_hdr.prev_part_uuid=2222222222222222 `
        pool_hdr.next_repl_uuid=1111111111111111 `
        pool_hdr.prev_repl_uuid=1111111111111111 `
        "pool_hdr.f:checksum_gen"

create_poolset $DIR\testset 20M:$DIR\testfile1 R 20M:$DIR\testfile2

expect_normal_exit $Env:EXE_DIR\log_pool$Env:EXESUFFIX o $DIR\testset

check

pass
