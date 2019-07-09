#
# Copyright 2015-2019, Intel Corporation
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

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

create_poolset $DIR\pool.set 32K:$DIR\testfile:x
expect_normal_exit $PMEMPOOL create obj $DIR\pool.obj

Set-Content -literalpath $DIR\not_pool.set -Value "NON_PMEMPOOLSET"
Set-Content -literalpath $DIR\too_short.set -Value "PMEMPOOL"

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
