# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019, Intel Corporation

#
# src/test/obj_root/TEST0 -- unit test for pmemobj_root
#

. ..\unittest\unittest.ps1

require_test_type medium

setup

# required free space is larger than file size, to be sure that the test
# will run
require_free_space 18G

create_holey_file 17G $DIR\testfile

expect_normal_exit $Env:EXE_DIR\obj_root$Env:EXESUFFIX $DIR\testfile

pass
