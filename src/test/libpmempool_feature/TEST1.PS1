# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019, Intel Corporation

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

# If SDS is not enabled at this point is because SDS is not available for
# this device
$ret = pmempool_feature_query_return "SHUTDOWN_STATE"
if ( $ret -eq "0" ) {
	msg "${Env:UNITTEST_NAME}: SKIP: SDS is not available"
	exit 0
}

# disable PMEMPOOL_FEAT_SHUTDOWN_STATE prior to success
$exit_func="expect_abnormal_exit"
libpmempool_feature_disable "CKSUM_2K" # should fail
$exit_func="expect_normal_exit"
libpmempool_feature_disable "SHUTDOWN_STATE"
libpmempool_feature_disable "CKSUM_2K" # should succeed

libpmempool_feature_enable "CKSUM_2K"

check

pass
