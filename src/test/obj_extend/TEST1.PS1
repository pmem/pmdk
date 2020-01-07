# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019, Intel Corporation

#
# src/test/obj_extend/TEST1 -- unit test for extending the pool
#

. ..\unittest\unittest.ps1

require_test_type medium
require_build_type debug
require_fs_type pmem

setup

$Env:RESVSIZE=$(((512 + 8) * 1024 * 1024)) # 512MiB + 8MiB
$Env:GRANULARITY=$((1024 * 1024 * 10)) # 10MiB

# prepare pool sets
create_poolset $DIR\testset2 `
	($Env:RESVSIZE + ":" + $DIR + "\testdir21:d") `
	O SINGLEHDR

$Env:PMEMOBJ_CONF += "heap.size.granularity=$Env:GRANULARITY;"
expect_normal_exit $Env:EXE_DIR\obj_extend$Env:EXESUFFIX $DIR\testset2

check

pass
