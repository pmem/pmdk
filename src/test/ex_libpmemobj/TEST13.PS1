# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# src/test/ex_libpmemobj/TEST13 -- unit test for libpmemobj examples
#

. ..\unittest\unittest.PS1

require_build_type debug nondebug
require_test_type medium
require_no_unicode

setup

expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_obj_pmemlog_minimal c $DIR\testfile1 `
	n t a:Hello a:World! n t w r n t v:Hello:World!:bar:foo n t w r `
	> out$Env:UNITTEST_NUM.log 2>&1

check

pass
