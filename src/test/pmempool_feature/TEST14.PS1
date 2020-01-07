# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019, Intel Corporation
#
#
# pmempool_feature/TEST14 -- unit test for SHUTDOWN_STATE + ctl
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup
. .\common.PS1

$Env:PMEMOBJ_CONF += "sds.at_create=0"
expect_normal_exit $PMEMPOOL create obj $POOLSET

pmempool_feature_query "SHUTDOWN_STATE"

check

pass
