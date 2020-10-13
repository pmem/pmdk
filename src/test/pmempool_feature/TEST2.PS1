# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2020, Intel Corporation
#
#
# pmempool_feature/TEST2 -- unit test for SHUTDOWN_STATE
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup
. .\common.PS1

expect_normal_exit $PMEMPOOL create obj $POOLSET

# PMEMPOOL_FEAT_SHUTDOWN_STATE is enabled by default
pmempool_feature_query "SHUTDOWN_STATE"

pmempool_feature_disable "SHUTDOWN_STATE"

# PMEMPOOL_FEAT_SHUTDOWN_STATE requires PMEMPOOL_FEAT_CHCKSUM_2K
pmempool_feature_disable "CKSUM_2K"
$exit_func="expect_abnormal_exit"
pmempool_feature_enable "SHUTDOWN_STATE" # should fail
$exit_func="expect_normal_exit"
pmempool_feature_enable "CKSUM_2K"
pmempool_feature_enable "SHUTDOWN_STATE" # should succeed

check

pass
