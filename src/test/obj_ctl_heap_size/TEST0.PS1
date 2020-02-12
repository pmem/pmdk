# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2019, Intel Corporation

#
# src/test/obj_ctl_heap_size/TEST0 -- unit test for obj_ctl_heap_size()
#

. ..\unittest\unittest.ps1

require_test_type short
require_fs_type any

setup

create_poolset $DIR\testset1 `
	4G:$DIR\testdir11:d `
	O SINGLEHDR

expect_normal_exit $PMEMPOOL$EXESUFFIX create obj --layout obj_ctl_heap_size `
	$DIR\testset1

# create pool sets
expect_normal_exit $Env:EXE_DIR\obj_ctl_heap_size$Env:EXESUFFIX $DIR\testset1 w

pass
