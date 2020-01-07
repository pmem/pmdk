# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# src/test/ex_libpmemobj/TEST7 -- unit test for libpmemobj examples
#

. ..\unittest\unittest.PS1

require_test_type medium
require_build_type debug nondebug
require_no_unicode

setup

@"
Hello!
"@| &$Env:EXAMPLES_DIR\ex_pmemobj_writer $DIR\testfile1 > out$Env:UNITTEST_NUM.log 2>&1
check_exit_code

expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_reader $DIR\testfile1 >> out$Env:UNITTEST_NUM.log 2>&1

check

pass
