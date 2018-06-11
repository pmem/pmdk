#
# Copyright 2014-2018, Intel Corporation
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
# src/test/util_file_open/TEST1 -- unit test for util_file_open()
#
[CmdletBinding(PositionalBinding=$false)]
Param(
    [alias("d")]
    $DIR = ""
    )

# standard unit test setup
. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any
require_no_superuser
# icacls does have problems with handling long paths in the correct way.
require_short_path

setup

create_holey_file 32K $DIR\testfile1

# remove write permissions
& icacls $DIR\testfile1 /deny ${Env:USERNAME}:W >$null

create_holey_file 32K $DIR\testfile2

# remove write permissions
& icacls $DIR\testfile2 /deny ${Env:USERNAME}:W >$null

expect_normal_exit $Env:EXE_DIR\util_file_open$Env:EXESUFFIX 0x4000 `
    $DIR\testfile1 `
    $DIR\testfile2

# grant full permissions so test code can cleanup
& icacls $DIR\testfile1 /grant ${Env:USERNAME}:F >$null
& icacls $DIR\testfile2 /grant ${Env:USERNAME}:F >$null

rm -Force $DIR\testfile1
rm -Force $DIR\testfile2

check

pass
