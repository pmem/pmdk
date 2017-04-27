#
# Copyright 2015-2017, Intel Corporation
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
# src/test/obj_check/TEST3 -- unit test for pmemobj_check
#
[CmdletBinding(PositionalBinding=$false)]
Param(
    [alias("d")]
    $DIR = ""
    )
$Env:UNITTEST_NAME = "obj_check\TEST3"
$Env:UNITTEST_NUM = "3"

# standard unit test setup
. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup
Remove-Item log$Env:UNITTEST_NUM.log -Force -Recurse -ea si
Remove-Item out$Env:UNITTEST_NUM.log -Force -Recurse -ea si
Remove-Item $DIR\testfile -Force -Recurse -ea si

# List lane section

# Two finish flags in redo log

expect_normal_exit $PMEMPOOL create obj $DIR\testfile
& $PMEMSPOIL $DIR\testfile "pmemobj.lane(0).list.redo_log(0).offset=0x1" "pmemobj.lane(0).list.redo_log(1).offset=0x1"
expect_normal_exit $Env:EXE_DIR\obj_check$Env:EXESUFFIX $DIR\testfile
Get-Content out$Env:UNITTEST_NUM.log | Out-File -append -encoding ascii log$Env:UNITTEST_NUM.log
Remove-Item $DIR\testfile -Force -Recurse
# Invalid offset in redo log - 4096
expect_normal_exit $PMEMPOOL create obj $DIR\testfile
& $PMEMSPOIL $DIR\testfile "pmemobj.lane(0).list.redo_log(0).offset=4096" "pmemobj.lane(0).list.redo_log(1).offset=0x1"
expect_normal_exit $Env:EXE_DIR\obj_check$Env:EXESUFFIX $DIR\testfile
Get-Content out$Env:UNITTEST_NUM.log | Out-File -append -encoding ascii log$Env:UNITTEST_NUM.log
Remove-Item $DIR\testfile -Force -Recurse

# Invalid offset in redo log - <file size>
expect_normal_exit $PMEMPOOL create obj $DIR\testfile
$SIZE=(get_size $DIR\testfile)
& $PMEMSPOIL $DIR\testfile "pmemobj.lane(0).list.redo_log(0).offset=$SIZE" "pmemobj.lane(0).list.redo_log(1).offset=0x1"
expect_normal_exit $Env:EXE_DIR\obj_check$Env:EXESUFFIX $DIR\testfile
Get-Content out$Env:UNITTEST_NUM.log | Out-File -append -encoding ascii log$Env:UNITTEST_NUM.log
Remove-Item $DIR\testfile -Force -Recurse

# Invalid obj_offset - 1024
expect_normal_exit $PMEMPOOL create obj $DIR\testfile
& $PMEMSPOIL $DIR\testfile "pmemobj.lane(0).list.obj_offset=1024"
expect_normal_exit $Env:EXE_DIR\obj_check$Env:EXESUFFIX $DIR\testfile
Get-Content out$Env:UNITTEST_NUM.log | Out-File -append -encoding ascii log$Env:UNITTEST_NUM.log
Remove-Item $DIR\testfile -Force -Recurse

# Invalid obj_offset - <file size>
expect_normal_exit $PMEMPOOL create obj $DIR\testfile
$SIZE=$(get_size $DIR\testfile)
& $PMEMSPOIL $DIR\testfile "pmemobj.lane(0).list.obj_offset=$SIZE"
expect_normal_exit $Env:EXE_DIR\obj_check$Env:EXESUFFIX $DIR\testfile
Get-Content out$Env:UNITTEST_NUM.log | Out-File -append -encoding ascii log$Env:UNITTEST_NUM.log

Move-Item log$Env:UNITTEST_NUM.log out$Env:UNITTEST_NUM.log -force

check

pass
