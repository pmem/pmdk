# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2020, Intel Corporation

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
