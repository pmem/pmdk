# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# src/test/ex_libpmem/TEST1 -- unit test for libpmem examples
#

. ..\unittest\unittest.ps1

require_test_type medium
require_no_unicode
require_short_path

setup

create_nonzeroed_file 2 0 $DIR\testfile1

expect_normal_exit $Env:EXAMPLES_DIR\ex_pmem_full_copy $DIR\testfile1 $DIR\testfile2 `
	> out$Env:UNITTEST_NUM.log 2>&1

cmp $DIR\testfile1 $DIR\testfile2 > cmp$Env:UNITTEST_NUM.log 2>&1

check

pass
