# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018, Intel Corporation
#
#
# pmempool_feature/TEST0 -- unit test for SINGLEHDR
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup
. .\common.PS1

expect_normal_exit $PMEMPOOL create obj $POOLSET

$exit_func="expect_abnormal_exit"
pmempool_feature_enable "SINGLEHDR" "no-query" # UNSUPPORTED
pmempool_feature_disable "SINGLEHDR" "no-query" # UNSUPPORTED
pmempool_feature_query "SINGLEHDR"

check

pass
