# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# src/test/ex_libpmemobj/TEST17 -- unit test for libpmemobj examples
#

. ..\unittest\unittest.PS1

require_test_type medium
require_no_unicode

setup

expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_lists $DIR\testfile1 1 foo 100 > out$Env:UNITTEST_NUM.log 2>&1
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_lists $DIR\testfile1 2 bar 200 >> out$Env:UNITTEST_NUM.log 2>&1
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_lists $DIR\testfile1 3 foo 300 >> out$Env:UNITTEST_NUM.log 2>&1
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_lists $DIR\testfile1 1 bar 400 >> out$Env:UNITTEST_NUM.log 2>&1
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_lists $DIR\testfile1 1 foo print >> out$Env:UNITTEST_NUM.log 2>&1
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_lists $DIR\testfile1 2 bar print >> out$Env:UNITTEST_NUM.log 2>&1
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_lists $DIR\testfile1 3 foo print >> out$Env:UNITTEST_NUM.log 2>&1
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_lists $DIR\testfile1 1 bar print >> out$Env:UNITTEST_NUM.log 2>&1

check

pass
