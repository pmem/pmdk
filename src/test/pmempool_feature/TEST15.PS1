# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019, Intel Corporation
#
#
# pmempool_feature/TEST15 -- unit test for SHUTDOWN_STATE + ctl
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup
. .\common.PS1

$Env:PMEMOBJ_CONF += "sds.at_create=1"
expect_normal_exit $PMEMPOOL create obj $POOLSET

# If SDS is not enabled at this point is because SDS is not available for
# this device
$ret = pmempool_feature_query_return "SHUTDOWN_STATE"
if ( $ret -eq "0" ) {
	msg "${Env:UNITTEST_NAME}: SKIP: SDS is not available"
	exit 0
}

pmempool_feature_query "SHUTDOWN_STATE"

check

pass
