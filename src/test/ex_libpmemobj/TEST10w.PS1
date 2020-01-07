# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# src/test/ex_libpmemobj/TEST10 -- unit test for libpmemobj examples
#

. ../unittest/unittest.PS1

require_test_type medium
require_build_type debug nondebug
require_no_unicode

setup

expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_obj_pmemblk c $DIR\testfile1 512 `
	n w:0:Hello w:13:World! w:20:foo w:30:bar `
	r:13 r:0 r:30 r:20 r:1 `
	z:20 z:30 `
	r:13 r:0 r:30 r:20 r:1 `
	e:13 r:13 w:13:rewrite r:13 `
	> out$Env:UNITTEST_NUM.log 2>$null

check

pass
