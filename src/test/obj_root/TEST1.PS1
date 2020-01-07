# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019, Intel Corporation

#
# src/test/obj_root/TEST1 -- unit test for pmemobj_root
#

. ..\unittest\unittest.ps1

# test type is long, because test freezes the system during execution
# on AppVeyor service
require_test_type long

setup

# required free space is larger than file size, to be sure that the test
# will run
require_free_space 18G

create_holey_file 17G $DIR\testfile

expect_normal_exit $Env:EXE_DIR\obj_root$Env:EXESUFFIX $DIR\testfile l

pass
