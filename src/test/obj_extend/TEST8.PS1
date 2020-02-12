# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019, Intel Corporation

#
# src/test/obj_extend/TEST8 -- negative unit test for reducing reservation size
#

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type any

setup

$Env:RESVSIZE=$((512 * 1024 * 1024)) # 512MiB
$Env:RESVSIZE_SMALL=$((256 * 1024 * 1024)) # 256MiB
$Env:ALLOCSIZE=$((10 * 1024 * 1024)) # 10MiB

# prepare pool sets
create_poolset $DIR\testset1 `
	($Env:RESVSIZE + ":" + $DIR + "\testdir10:d") `
	O SINGLEHDR

# prepare pool sets
create_poolset $DIR\testset2 `
	($Env:RESVSIZE_SMALL + ":" + $DIR + "\testdir10:d") `
	O SINGLEHDR

expect_normal_exit $Env:EXE_DIR\obj_extend$Env:EXESUFFIX $DIR\testset1 $Env:ALLOCSIZE $DIR\testset2

check

pass
