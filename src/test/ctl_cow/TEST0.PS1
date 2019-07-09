#
# Copyright 2019, Intel Corporation
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
# src/test/ctl_cow/TEST0 -- unit test which check copy on write feature
# for obj pools

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type any

setup

$POOL="$DIR\testfile"
$REPLICA="$DIR\testfile_replica"
$POOLSET="$DIR\pool0.set"

create_poolset $POOLSET `
    32M:$DIR\testfile:z `
	R `
    32M:$DIR\testfile_replica:z

expect_normal_exit $PMEMPOOL create obj $POOLSET

$POOL_BEFORE = $(Get-FileHash $POOL).hash
$REPLICA_BEFORE = $(Get-FileHash $REPLICA).hash

$Env:PMEMOBJ_CONF += "copy_on_write.at_open=1"

expect_normal_exit $Env:EXE_DIR\ctl_cow$Env:EXESUFFIX $POOLSET obj

$POOL_AFTER = $(Get-FileHash $POOL).hash
$REPLICA_AFTER = $(Get-FileHash $REPLICA).hash

if ($POOLSET_BEFORE -ne $POOLSET_AFTER -Or $REPLICA_BEFORE -ne $REPLICA_AFTER) {
	fatal '$POOLSET_BEFORE -ne $POOLSET_AFTER -or $REPLICA_BEFORE -ne $REPLICA_AFTER'
}
check

pass
