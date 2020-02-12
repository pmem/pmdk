# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation

#
# src/test/traces_pmem/TEST0 -- unit test traces for libraries pmem
#

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type none
require_build_type debug static-debug

setup

$Env:PMEM_LOG_LEVEL = 4
$Env:PMEM_LOG_FILE = ".\trace_pmem$Env:UNITTEST_NUM.log"
$Env:PMEMBLK_LOG_LEVEL = 4
$Env:PMEMBLK_LOG_FILE = ".\trace_pmemblk$Env:UNITTEST_NUM.log"
$Env:PMEMLOG_LOG_LEVEL = 4
$Env:PMEMLOG_LOG_FILE = ".\trace_pmemlog$Env:UNITTEST_NUM.log"
$Env:PMEMOBJ_LOG_LEVEL = 4
$Env:PMEMOBJ_LOG_FILE = ".\trace_pmemobj$Env:UNITTEST_NUM.log"

expect_normal_exit $Env:EXE_DIR\traces_pmem$Env:EXESUFFIX

# We want to check if traces are enabled and correctness of printed
# string format. We can't rely on full output (because of different
# machines configurations), hence comparison of only first 3 lines.
Get-Content $Env:PMEM_LOG_FILE -TotalCount 3 | Set-Content ./trace_pmem_head$Env:UNITTEST_NUM.log
Get-Content $Env:PMEMBLK_LOG_FILE -TotalCount 3 | Set-Content ./trace_pmemblk_head$Env:UNITTEST_NUM.log
Get-Content $Env:PMEMLOG_LOG_FILE -TotalCount 3 | Set-Content ./trace_pmemlog_head$Env:UNITTEST_NUM.log
Get-Content $Env:PMEMOBJ_LOG_FILE -TotalCount 3 | Set-Content ./trace_pmemobj_head$Env:UNITTEST_NUM.log

check

pass
