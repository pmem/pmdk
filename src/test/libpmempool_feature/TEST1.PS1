#
# Copyright 2018-2019, Intel Corporation
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
# libpmempool_feature/TEST1 -- unit test for PMEMPOOL_FEAT_CHCKSUM_2K
#

. ..\unittest\unittest.ps1

require_test_type medium

# we are matching pmempool logs which are available only in debug version
require_build_type debug

require_fs_type any

setup
. .\common.ps1

expect_normal_exit $PMEMPOOL create obj $POOL

# PMEMPOOL_FEAT_CHCKSUM_2K is enabled by default
libpmempool_feature_query "CKSUM_2K"

# disable PMEMPOOL_FEAT_SHUTDOWN_STATE prior to success
$exit_func="expect_abnormal_exit"
libpmempool_feature_disable "CKSUM_2K" # should fail
$exit_func="expect_normal_exit"
libpmempool_feature_disable "SHUTDOWN_STATE"
libpmempool_feature_disable "CKSUM_2K" # should succeed

libpmempool_feature_enable "CKSUM_2K"

check

pass
