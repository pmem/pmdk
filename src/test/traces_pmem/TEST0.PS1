#
# Copyright 2016-2019, Intel Corporation
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#
#     * Neither the name of the copyright holder nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

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
