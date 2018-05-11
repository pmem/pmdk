#
# Copyright 2016-2018, Intel Corporation
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
# libpmempool_rm/TEST0 -- test pmempool_rm with single-file pools
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

enable_log_append

expect_normal_exit $PMEMPOOL$EXESUFFIX create blk 512 $DIR\pool.blk
expect_normal_exit $PMEMPOOL$EXESUFFIX create log $DIR\pool.log
expect_normal_exit $PMEMPOOL$EXESUFFIX create obj $DIR\pool.obj
check_files $DIR\pool.blk $DIR\pool.log $DIR\pool.obj

# this should remove all the pool files
expect_normal_exit $Env:EXE_DIR\libpmempool_rm$Env:EXESUFFIX $DIR\pool.blk $DIR\pool.log $DIR\pool.obj

# check if all pool files are removed
check_no_files $DIR\pool.blk $DIR\pool.log $DIR\pool.obj

# this should report errors for all files
expect_normal_exit $Env:EXE_DIR\libpmempool_rm$Env:EXESUFFIX $DIR\pool.blk $DIR\pool.log $DIR\pool.obj
# this should ignore all errors
expect_normal_exit $Env:EXE_DIR\libpmempool_rm$Env:EXESUFFIX -f $DIR\pool.blk $DIR\pool.log $DIR\pool.obj

check

pass
