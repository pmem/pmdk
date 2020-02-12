# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019, Intel Corporation

#
# src/test/obj_mem/TEST0 -- test for pmemobj_memcpy, pmemobj_memmove
# and pmemobj_memset
#

. ..\unittest\unittest.ps1

require_test_type medium

setup

create_poolset $DIR\poolset1 16M:$DIR\testfile1:z
expect_normal_exit $Env:EXE_DIR\obj_mem$Env:EXESUFFIX $DIR/poolset1

create_poolset $DIR\poolset2 16M:$DIR\testfile2:z r 16M:$DIR\testfile3:z
expect_normal_exit $Env:EXE_DIR\obj_mem$Env:EXESUFFIX $DIR/poolset2

pass
