# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018, Intel Corporation
#
#
# pmempool_feature/TEST13 -- unit test for CHECK_BAD_BLOCKS
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup
. .\common.PS1

expect_normal_exit $PMEMPOOL create obj $POOLSET

# PMEMPOOL_FEAT_CHECK_BAD_BLOCKS is disabled by default
pmempool_feature_query "CHECK_BAD_BLOCKS"

$exit_func="expect_abnormal_exit"
pmempool_feature_enable "CHECK_BAD_BLOCKS" # should fail

$exit_func="expect_normal_exit"
pmempool_feature_disable "CHECK_BAD_BLOCKS" # should succeed

check

pass
