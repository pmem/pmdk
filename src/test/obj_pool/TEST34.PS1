# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019, Intel Corporation

#
# src/test/obj_pool/TEST34 -- unit test for pmemobj_create
#

. ..\unittest\unittest.ps1

require_test_type long

setup

require_free_space 33G

#
# TEST0 non-existing file, poolsize > 0
#
expect_normal_exit $Env:EXE_DIR\obj_pool$Env:EXESUFFIX c $DIR\testfile test$Env:SUFFIX 32768 0600

check_files $DIR\testfile

check

pass
