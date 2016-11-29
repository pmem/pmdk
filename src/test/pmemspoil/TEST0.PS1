#
# Copyright 2016, Intel Corporation
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
# src\test\pmemspoil\TEST0.PS1 -- test for pmemspoil
#

[CmdletBinding(PositionalBinding=$false)]
Param(
    [alias("d")]
    $DIR = ""
    )
$ENV:UNITTEST_NAME = "pmemspoil\TEST0"
$ENV:UNITTEST_NUM = "0"

# standard unit test setup
. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type any

setup

$LOG="out$Env:UNITTEST_NUM.log"
rm $LOG -Force -ea si

$POOLSET="$DIR\pool.set"
$POOL1="$DIR\pool.part1"
$POOL2="$DIR\pool.part2"
$POOL3="$DIR\pool.part3"
$REPL1="$DIR\pool.rep.part1"
$REPL2="$DIR\pool.rep.part2"
$REPL3="$DIR\pool.rep.part3"
$POOLS="$POOL1 $POOL2 $POOL3 $REPL1 $REPL2 $REPL3"

create_poolset $POOLSET `
		32M:$POOL1 32M:$POOL2 32M:$POOL3 `
		R `
		32M:$REPL1 32M:$REPL2 32M:$REPL3
check_file $POOLSET

rm $POOLS -ea ig
expect_normal_exit $PMEMPOOL create obj --layout pmempool $POOLSET
&$PMEMSPOIL $POOLSET --replica 0 "pmemobj.layout=replica0"
expect_normal_exit $PMEMPOOL info --replica 0 $POOLSET -f obj > $LOG
&$PMEMSPOIL $POOLSET --replica 1 "pmemobj.layout=replica1"
expect_normal_exit $PMEMPOOL info --replica 1 $POOLSET -f obj >> $LOG

Get-Content $LOG | Where-Object {$_ -match 'Layout'} > tmp$LOG
Move-Item -Force tmp$LOG $LOG

check

pass
