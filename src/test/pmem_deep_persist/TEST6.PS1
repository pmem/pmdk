# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2020, Intel Corporation

#
# src/test/pmem_deep_persist/TEST6 -- unit test for pmem_deep_persist
#

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type pmem non-pmem
require_build_type debug

setup

$Env:PMEMOBJ_LOG_LEVEL=15

create_poolset $DIR\testset1 16M:$DIR\testfile1 16M:$DIR\testfile2 r `
	64M:$DIR\testfile3

$Env:DEEP_PERSIST_SIZE=$(convert_to_bytes 16M)
$Env:OFFSET=$(convert_to_bytes 8M)

expect_normal_exit $Env:EXE_DIR\pmem_deep_persist$Env:EXESUFFIX $DIR\testset1 o $Env:DEEP_PERSIST_SIZE $Env:OFFSET

$FileContent = Get-Content "pmemobj$Env:UNITTEST_NUM.log"

$Matches = $FileContent | Select-String -Pattern "<libpmemobj>: <3> .*os_part_deep_common.*"
$Matches.Matches.Count > grep$Env:UNITTEST_NUM.log

# Instead of count deep_flush_write
echo 0 >> grep$Env:UNITTEST_NUM.log

check

pass
