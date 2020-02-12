# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation

#
# src/test/ex_libpmemobj/TEST20 -- unit test for libpmemobj examples
#

. ..\unittest\unittest.PS1

require_test_type medium
require_build_type debug nondebug
require_no_unicode

setup

expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_array $DIR\testfile alloc int 23 int > out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_array $DIR\testfile alloc PMEMoid 17 PMEMoid >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_array $DIR\testfile alloc TOID 11 TOID >> out$Env:UNITTEST_NUM.log

expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_array $DIR\testfile print int >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_array $DIR\testfile print PMEMoid >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_array $DIR\testfile print TOID >> out$Env:UNITTEST_NUM.log

expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_array $DIR\testfile realloc int 17 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_array $DIR\testfile realloc PMEMoid 11 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_array $DIR\testfile realloc TOID 13 >> out$Env:UNITTEST_NUM.log

expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_array $DIR\testfile print int >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_array $DIR\testfile print PMEMoid >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_array $DIR\testfile print TOID >> out$Env:UNITTEST_NUM.log

check

pass
