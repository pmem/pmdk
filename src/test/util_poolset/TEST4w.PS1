#
# Copyright 2017, Intel Corporation
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
# src/test/util_poolset/TEST4w -- unit test for util_pool_open()
#
[CmdletBinding(PositionalBinding=$false)]
Param(
    [alias("d")]
    $DIR = ""
    )
$Env:UNITTEST_NAME = "util_poolset/TEST4w"
$Env:UNITTEST_NUM = "4w"

# standard unit test setup
. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type non-pmem

setup

$Env:TEST_LOG_LEVEL = "4"
$Env:TEST_LOG_FILE = "test$Env:UNITTEST_NUM.log"

$MIN_POOL = 128 * 1024 * 1024 # 128MiB
$MIN_POOL_STR = ${MIN_POOL}.toString() + "B" # 128MiB
$RESVSIZE=4 * 1024 * 1024 * 1024 # 4GiB
$RESVSIZE_STR = ${RESVSIZE}.toString() + "B" # 4GiB

create_poolset $DIR\testset1 `
	${RESVSIZE_STR}:$DIR\testdir11:d
create_poolset $DIR\testset2 `
	${RESVSIZE_STR}:$DIR\testdir21:d ${RESVSIZE_STR}:$DIR\testdir22:d
create_poolset $DIR\testset3 `
	${RESVSIZE_STR}:$DIR\testdir31:d R ${RESVSIZE_STR}:$DIR\testdir32:d
create_poolset $DIR\testset4 `
	${RESVSIZE_STR}:$DIR\testdir41:d ${RESVSIZE_STR}:$DIR\testdir42:d `
	R ${RESVSIZE_STR}:$DIR\testdir43:d ${RESVSIZE_STR}:$DIR\testdir44:d

create_poolset $DIR\testset5 `
	${RESVSIZE_STR}:$DIR\testdir51:d `
	O NOHDRS
create_poolset $DIR\testset6 `
	${RESVSIZE_STR}:$DIR\testdir61:d ${RESVSIZE_STR}:$DIR\testdir62:d `
	O NOHDRS
create_poolset $DIR\testset7 `
	${RESVSIZE_STR}:$DIR\testdir71:d R ${RESVSIZE_STR}:$DIR\testdir72:d `
	O NOHDRS
create_poolset $DIR\testset8 `
	${RESVSIZE_STR}:$DIR\testdir81:d ${RESVSIZE_STR}:$DIR\testdir82:d `
	R ${RESVSIZE_STR}:$DIR\testdir83:d ${RESVSIZE_STR}:$DIR\testdir84:d `
	O NOHDRS

# create pool sets
expect_normal_exit $Env:EXE_DIR\util_poolset$Env:EXESUFFIX c $MIN_POOL `
    $DIR\testset1 `
    $DIR\testset2 `
    $DIR\testset3 `
    $DIR\testset4 `
    $DIR\testset5 `
    $DIR\testset6 `
    $DIR\testset7 `
    $DIR\testset8

expect_normal_exit $Env:EXE_DIR\util_poolset$Env:EXESUFFIX o $MIN_POOL `
    $DIR\testset1 `
    $DIR\testset2 `
    $DIR\testset3 `
    $DIR\testset4 `
    $DIR\testset5 `
    $DIR\testset6 `
    $DIR\testset7 `
    $DIR\testset8

expect_normal_exit $Env:EXE_DIR\util_poolset$Env:EXESUFFIX e $MIN_POOL `
    $DIR\testset1 `
    $DIR\testset2 `
    $DIR\testset3 `
    $DIR\testset4 `
    $DIR\testset5 `
    $DIR\testset6 `
    $DIR\testset7 `
    $DIR\testset8

expect_normal_exit $Env:EXE_DIR\util_poolset$Env:EXESUFFIX e $MIN_POOL `
    $DIR\testset1 `
    $DIR\testset2 `
    $DIR\testset3 `
    $DIR\testset4 `
    $DIR\testset5 `
    $DIR\testset6 `
    $DIR\testset7 `
    $DIR\testset8

expect_normal_exit $Env:EXE_DIR\util_poolset$Env:EXESUFFIX e $MIN_POOL `
    $DIR\testset1 `
    $DIR\testset2 `
    $DIR\testset3 `
    $DIR\testset4 `
    $DIR\testset5 `
    $DIR\testset6 `
    $DIR\testset7 `
    $DIR\testset8

expect_normal_exit $Env:EXE_DIR\util_poolset$Env:EXESUFFIX o $MIN_POOL `
    $DIR\testset1 `
    $DIR\testset2 `
    $DIR\testset3 `
    $DIR\testset4 `
    $DIR\testset5 `
    $DIR\testset6 `
    $DIR\testset7 `
    $DIR\testset8

sls -Path $Env:TEST_LOG_FILE -Pattern "<1>" | `
    %{[string]$_ -replace '^.* len ',"" -replace '^.*][ ]*',''} `
    > grep$Env:UNITTEST_NUM.log

check

pass
