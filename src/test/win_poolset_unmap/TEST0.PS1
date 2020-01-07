# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019, Intel Corporation

#
# src/test/win_poolset_unmap/TEST0 -- unit test for win_mmap
#

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type any
require_build_type debug

setup

$Env:PMEM_LOG_LEVEL=15

mkdir $DIR\dirset | Out-Null

create_poolset $DIR\testset 50M:$DIR\dirset O SINGLEHDR

expect_normal_exit $Env:EXE_DIR\win_poolset_unmap$Env:EXESUFFIX $DIR\testset

Get-Content pmem$Env:UNITTEST_NUM.log | Where-Object `
	{$_ -match "win_mmap.c:[0-9]* mmap_unreserve"} > grep$Env:UNITTEST_NUM.log

check

pass
