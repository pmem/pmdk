# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019, Intel Corporation

#
# libpmempool_feature/TEST0 -- unit test for PMEMPOOL_FEAT_SINGLEHDR
#

. ..\unittest\unittest.ps1

require_test_type medium

# we are matching pmempool logs which are available only in debug version
require_build_type debug

require_fs_type any

setup
. .\common.PS1

expect_normal_exit $PMEMPOOL create obj $POOL

$exit_func="expect_abnormal_exit"
libpmempool_feature_enable "SINGLEHDR" "no-query" # UNSUPPORTED
libpmempool_feature_disable "SINGLEHDR" "no-query" # UNSUPPORTED
libpmempool_feature_query "SINGLEHDR"

check

pass
