# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2020, Intel Corporation

#
# libpmempool_feature/TEST2 -- unit test for PMEMPOOL_FEAT_SHUTDOWN_STATE
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup
. .\common.ps1

expect_normal_exit $PMEMPOOL create obj $POOL

# PMEMPOOL_FEAT_SHUTDOWN_STATE is enabled by default
libpmempool_feature_query "SHUTDOWN_STATE"

libpmempool_feature_disable "SHUTDOWN_STATE"

# PMEMPOOL_FEAT_SHUTDOWN_STATE requires PMEMPOOL_FEAT_CHCKSUM_2K
libpmempool_feature_disable "CKSUM_2K"
$exit_func="expect_abnormal_exit"
libpmempool_feature_enable "SHUTDOWN_STATE" # should fail
$exit_func="expect_normal_exit"
libpmempool_feature_enable "CKSUM_2K"
libpmempool_feature_enable "SHUTDOWN_STATE" # should succeed

check

pass
