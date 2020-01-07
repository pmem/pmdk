# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019, Intel Corporation

#
# src/test/pmem_deep_persist/TEST4 -- unit test for pmem_deep_persist
#

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type pmem non-pmem
require_build_type debug

setup

$Env:PMEMOBJ_LOG_LEVEL=15

truncate -s 2M $DIR\testfile1

$Env:DEEP_PERSIST_SIZE=$(convert_to_bytes 2M)

expect_normal_exit $Env:EXE_DIR\pmem_deep_persist$Env:EXESUFFIX $DIR\testfile1 m $Env:DEEP_PERSIST_SIZE 0

$FileContent = Get-Content "pmemobj$Env:UNITTEST_NUM.log"

$Matches = $FileContent | Select-String -Pattern "<libpmemobj>: <15>.*pmem\.c.*pmem_msync.*"
$Matches.Matches.Count > grep$Env:UNITTEST_NUM.log

# Instead of count deep_flush_write
echo 0 >> grep$Env:UNITTEST_NUM.log

check

pass
