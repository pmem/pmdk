#
# Copyright 2015-2016, Intel Corporation
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
# src/test/util_is_poolset/TEST0 -- unit test for util_is_poolset
#
#
# parameter handling
#
[CmdletBinding(PositionalBinding=$false)]
Param(
    [alias("d")]
    $DIR = ""
    )
$Env:UNITTEST_NAME = "util_is_poolset\TEST0"
$Env:UNITTEST_NUM = "0"
# XXX:  bash has a few calls to tools that we don't have on
# windows (yet) that set PMEM_IS_PMEM and NON_PMEM_IS_PMEM based
# on their output
$Env:PMEM_IS_PMEM = $true
$Env:NON_PMEM_IS_PMEM = $true

# standard unit test setup
. ..\unittest\unittest.ps1

require_fs_type any

setup

create_poolset $DIR\pool.set 32K:$DIR\testfile:x
expect_normal_exit $PMEMPOOL$EXESUFFIX create obj $DIR\pool.obj
$sig = New-Object Byte[] 14
$sig[0] = 78 # 0x4e N
$sig[1] = 79 # 0x4f O
$sig[2] = 78 # 0x4e N
$sig[3] = 95 # 0x5f _
$sig[4] = 77 # 0x4d M
$sig[5] = 69 # 0x45 E
$sig[6] = 77 # 0x4d M
$sig[7] = 80 # 0x50 P
$sig[8] = 79 # 0x4f O
$sig[9] = 79 # 0x4f O
$sig[10] = 76 # 0x4c L
$sig[11] = 83 # 0x53 S
$sig[12] = 69 # 0x45 E
$sig[13] = 84 # 0x54 T
Set-Content -Path $DIR\not_pool.set -Value $sig -Encoding Byte

$sig = New-Object Byte[] 8
$sig[0] = 80 # 0x50 P
$sig[1] = 77 # 0x4d M
$sig[2] = 69 # 0x45 E
$sig[3] = 77 # 0x4d M
$sig[4] = 80 # 0x50 P
$sig[5] = 79 # 0x4f O
$sig[6] = 79 # 0x4f O
$sig[7] = 76 # 0x4c L
Set-Content -Path $DIR\too_short.set -Value $sig -Encoding Byte
touch $DIR\empty.set

expect_normal_exit $Env:EXE_DIR\util_is_poolset$Env:EXESUFFIX `
    $DIR\pool.set `
    $DIR\not_pool.set `
    $DIR\too_short.set `
    $DIR\pool.obj `
    $DIR\empty.set `
    $DIR\not_exist.set

check

pass
