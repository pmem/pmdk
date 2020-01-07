# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2018, Intel Corporation
#
#
# pmempool_info/TEST9 -- test for info command
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$POOL="$DIR\file.pool"

expect_normal_exit $PMEMPOOL create -w blk 512 $POOL
$INFO_NFLOG=((&$PMEMPOOL info $POOL | sls 'Free block').Line | sls -Pattern '\d+').Matches.Value
$NFLOG=(expect_normal_exit $PMEMPOOL info -g $POOL | sls -Pattern '^\d+:'|measure).count

if ($INFO_NFLOG -ne $NFLOG ) {
    echo "$INFO_NFLOG != $NFLOG"
    fail 1
}

pass
