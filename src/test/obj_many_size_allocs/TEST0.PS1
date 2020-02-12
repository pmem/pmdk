# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018, Intel Corporation

#
# src/test/obj_many_size_allocs/TEST0 -- test for allocation of many
#										objects with different sizes
#

. ..\unittest\unittest.ps1

require_test_type long

setup

# limit number of emitted logs due to excessive log size
$Env:PMEMOBJ_LOG_LEVEL=2

create_holey_file 16M $DIR\testfile1

expect_normal_exit $Env:EXE_DIR\obj_many_size_allocs$Env:EXESUFFIX $DIR\testfile1

check

pass
