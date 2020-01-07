# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019, Intel Corporation

#
# src/test/obj_extend/TEST4 -- unit test for extending the pool
# negative scenario - mixed directories and files
# (pmem/issues#820)
#

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type any

setup

$Env:RESVSIZE="10M"

# prepare pool sets (master: dir; replica: file)
create_poolset $DIR\testset1 `
	($Env:RESVSIZE + ":" + $DIR + "\testdir11:d") `
	R `
	($Env:RESVSIZE + ":" + $DIR + "\testfile21") `
	O SINGLEHDR

expect_normal_exit $Env:EXE_DIR\obj_extend$Env:EXESUFFIX $DIR\testset1

check

pass
