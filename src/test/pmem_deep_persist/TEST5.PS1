#
# Copyright 2018-2019, Intel Corporation
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
# src/test/pmem_deep_persist/TEST5 -- unit test for pmem_deep_persist
#

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type pmem non-pmem
require_build_type debug

setup

$Env:PMEMOBJ_LOG_LEVEL=15

create_poolset $DIR\testset1 16M:$DIR\testfile1 16M:$DIR\testfile2 r `
	64M:$DIR\testfile3

$Env:DEEP_PERSIST_SIZE=$(convert_to_bytes 2M)

expect_normal_exit $Env:EXE_DIR\pmem_deep_persist$Env:EXESUFFIX $DIR\testset1 o $Env:DEEP_PERSIST_SIZE 4096

$FileContent = Get-Content "pmemobj$Env:UNITTEST_NUM.log"

$Matches = $FileContent | Select-String -Pattern "<libpmemobj>: <3> .*os_part_deep_common.*"
$Matches.Matches.Count > grep$Env:UNITTEST_NUM.log

# Instead of count deep_flush_write
echo 0 >> grep$Env:UNITTEST_NUM.log

check

pass
