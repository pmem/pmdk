# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# src/test/obj_list_recovery/TEST1 -- unit test for list recovery process
#

. ..\unittest\unittest.ps1

require_test_type medium
require_build_type debug

setup

create_holey_file 1M $DIR\testfile

rm -Force log$Env:UNITTEST_NUM.log -ErrorAction SilentlyContinue

function concat_log {
	cat out$Env:UNITTEST_NUM.log | out-file -append -encoding ascii -literalpath log$Env:UNITTEST_NUM.log
	rm out$Env:UNITTEST_NUM.log
}

# fail after finish flag is set in redo log
expect_normal_exit $Env:EXE_DIR\obj_list$Env:EXESUFFIX $DIR\testfile `
		i:1:0 i:1:0 `
		P:2 R:2 `
		F:after_finish `
		r:0

concat_log

# check
expect_normal_exit $Env:EXE_DIR\obj_list$Env:EXESUFFIX $DIR\testfile `
		P:2 R:2 `
		V `
		P:2 R:2

concat_log

# fail before finish flag is set in redo log
expect_normal_exit $Env:EXE_DIR\obj_list$Env:EXESUFFIX $DIR\testfile `
		F:before_finish `
		r:0

concat_log

# check
expect_normal_exit $Env:EXE_DIR\obj_list$Env:EXESUFFIX $DIR\testfile `
		P:2 R:2 `
		V `
		P:2 R:2

concat_log

check

pass
