# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019, Intel Corporation
#
#
# pmempool_create/TEST13 -- testing if file allocation fails when
# file already exists
#

. ../unittest/unittest.ps1

require_test_type medium

require_fs_type any

setup

expect_normal_exit $PMEMPOOL$EXESUFFIX create log $DIR\pool.log

expect_abnormal_exit $PMEMPOOL$EXESUFFIX create -M log $DIR\pool.log 2>&1 | `
		select-String -Pattern "File|exists" > grep13w.log
pass
