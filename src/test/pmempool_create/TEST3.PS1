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
# pmempool_create/TEST3 -- test for inheriting pools
#
[CmdletBinding(PositionalBinding=$false)]
Param(
    [alias("d")]
    $DIR = ""
    )


. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$POOL="$DIR\file.pool"
$INHERIT="$DIR\inherit.pool"

# PMEMBLK
expect_normal_exit $PMEMPOOL create blk 512 $POOL
check_file $POOL
check_signature PMEMBLK $POOL
sv -Name SIZE -Scope "Local" (get_size $POOL)
expect_normal_exit $PMEMPOOL create --inherit $POOL $INHERIT
check_file $INHERIT
check_signature PMEMBLK $INHERIT
check_size $SIZE $INHERIT

# PMEMLOG
rm $POOL
rm $INHERIT
expect_normal_exit $PMEMPOOL create log $POOL
check_file $POOL
check_signature PMEMLOG $POOL
sv -Name SIZE -Scope "Local" (get_size $POOL)
expect_normal_exit $PMEMPOOL create --inherit $POOL $INHERIT
check_file $INHERIT
check_signature PMEMLOG $INHERIT
check_size $SIZE $INHERIT

# PMEMOBJ
rm $POOL
rm $INHERIT
expect_normal_exit $PMEMPOOL create --layout=pmempool$Env:SUFFIX obj $POOL
check_file $POOL
check_signature PMEMOBJ $POOL
check_layout pmempool$Env:SUFFIX $POOL
sv -Name SIZE -Scope "Local" (get_size $POOL)
expect_normal_exit $PMEMPOOL create --inherit $POOL $INHERIT
check_file $INHERIT
check_signature PMEMOBJ $INHERIT
check_layout pmempool$Env:SUFFIX $POOL
check_size $SIZE $INHERIT

pass
