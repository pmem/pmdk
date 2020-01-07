# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation
#
#
# src\test\util_poolset_size\TEST0 -- unit test for util_poolset_size
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

create_poolset $DIR\pool.set1 32K:$DIR\testfile:x
create_poolset $DIR\pool.set2 32K:$DIR\testfile1:x 32M:$DIR\testfile2:x
create_poolset $DIR\pool.set3 `
	32M:$DIR\testfile11:x 32M:$DIR\testfile12:x `
	R 64M:$DIR\testfile21:x 32M:$DIR\testfile22:x
create_poolset $DIR\pool.set4 `
	32M:$DIR\testfile11:x 32M:$DIR\testfile12:x `
	R 64M:$DIR\testfile21:x 32M:$DIR\testfile22:x `
	R 64M:$DIR\testfile31:x 64M:$DIR\testfile32:x

expect_normal_exit $Env:EXE_DIR\util_poolset_size$Env:EXESUFFIX `
	$DIR\pool.set1 `
	$DIR\pool.set2 `
	$DIR\pool.set3 `
	$DIR\pool.set4

check

pass
