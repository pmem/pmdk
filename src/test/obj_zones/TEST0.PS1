# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019, Intel Corporation

#
# src/test/obj_zones/TEST0 -- utnit test for allocates
# from a very large pool (exceeding 1 zone)
#

. ..\unittest\unittest.ps1

# too large

require_test_type medium

setup

create_holey_file 64G $DIR\testfile1

expect_normal_exit $Env:EXE_DIR\obj_zones$Env:EXESUFFIX $DIR\testfile1 c

check

expect_normal_exit $Env:EXE_DIR\obj_zones$Env:EXESUFFIX $DIR\testfile1 o

pass
