#
# Copyright 2017-2019, Intel Corporation
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
# src/test/util_poolset/TEST6w -- unit test for util_pool_open()
#

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type non-pmem

setup

$Env:TEST_LOG_LEVEL = "6"
$Env:TEST_LOG_FILE = "test$Env:UNITTEST_NUM.log"

$MIN_POOL = 128 * 1024 * 1024 # 4MiB
$MIN_POOL_STR = ${MIN_POOL}.toString() + "B" # 128MiB
$RESVSIZE=260 * 1024 * 1024 # 260 MiB
$RESVSIZE_STR = ${RESVSIZE}.toString() + "B" # 260 MiB

$REQUIRE_POOL = ${MIN_POOL} * 8
require_free_space "${REQUIRE_POOL}b"

# prepare pool sets
create_poolset $DIR\testset2 `
	${RESVSIZE_STR}:$DIR\testdir21:d ${RESVSIZE_STR}:$DIR\testfile22
create_poolset $DIR\testset3 `
	${RESVSIZE_STR}:$DIR\testdir31:d R ${RESVSIZE_STR}:$DIR\testfile32
create_poolset $DIR\testset4 `
	${RESVSIZE_STR}:$DIR\testdir41:d ${RESVSIZE_STR}:$DIR\testdir42:d `
	R ${RESVSIZE_STR}:$DIR\testdir43:d ${RESVSIZE_STR}:$DIR\testfile44

create_poolset $DIR\testset6 `
	${RESVSIZE_STR}:$DIR\testdir61:d ${RESVSIZE_STR}:$DIR\testfile62 `
	O SINGLEHDR
create_poolset $DIR\testset7 `
	${RESVSIZE_STR}:$DIR\testdir71:d R ${RESVSIZE_STR}:$DIR\testfile72 `
	O SINGLEHDR
create_poolset $DIR\testset8 `
	${RESVSIZE_STR}:$DIR\testdir81:d ${RESVSIZE_STR}:$DIR\testdir82:d `
	R ${RESVSIZE_STR}:$DIR\testdir83:d ${RESVSIZE_STR}:$DIR\testfile84 `
	O SINGLEHDR

create_poolset $DIR\testset9 `
	${RESVSIZE_STR}:$DIR\testdir91 ${RESVSIZE_STR}:$DIR\testfile92:d `
	O SINGLEHDR

# create pool sets
expect_normal_exit $Env:EXE_DIR\util_poolset$Env:EXESUFFIX c $MIN_POOL `
    $DIR\testset2 `
    $DIR\testset3 `
    $DIR\testset4 `
    $DIR\testset6 `
    $DIR\testset7 `
    $DIR\testset8 `
    $DIR\testset9

sls -Path $Env:TEST_LOG_FILE -Pattern "<1>" | `
    %{[string]$_ -replace '^.* len ',"" -replace '^.*][ ]*',''} `
    > grep$Env:UNITTEST_NUM.log

check

pass
