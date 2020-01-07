# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# src/test/ex_libpmemobj/TEST1 -- unit test for libpmemobj examples
#

. ../unittest/unittest.PS1

require_test_type medium
require_build_type debug nondebug
require_no_unicode

setup

expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_btree $DIR\testfile1 i 1234 foo > out$Env:UNITTEST_NUM.log 2>&1
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_btree $DIR\testfile1 i 4321 bar >> out$Env:UNITTEST_NUM.log 2>&1
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_btree $DIR\testfile1 i 1 Hello >> out$Env:UNITTEST_NUM.log 2>&1
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_btree $DIR\testfile1 i 2 World! >> out$Env:UNITTEST_NUM.log 2>&1
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_btree $DIR\testfile1 p >> out$Env:UNITTEST_NUM.log 2>&1
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_btree $DIR\testfile1 f 4321 >> out$Env:UNITTEST_NUM.log 2>&1

check

pass
