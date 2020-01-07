# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# src/test/ex_libpmemobj/TEST19 -- unit test for libpmemobj examples
#

. ..\unittest\unittest.PS1

require_test_type medium
require_build_type debug nondebug
require_no_unicode

setup

@"
i 1234
i 4321
c 1234
r 1234
c 4321
r 4321
c 4321
q
"@ | &$Env:EXAMPLES_DIR\ex_pmemobj_mapcli skiplist $DIR\testfile1 555 > out$Env:UNITTEST_NUM.log 2>&1

check_exit_code

check

pass
