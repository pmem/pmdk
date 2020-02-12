# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2019, Intel Corporation

#
# src/test/obj_sds/TEST5 -- unittest for shutdown state
#

. ..\unittest\unittest.PS1

require_test_type medium

require_fs_type any

require_build_type debug

setup

$POOLSET="$DIR\pool.set"
# Create poolset file
create_poolset $POOLSET `
			   20M:$DIR\testfile01:x `
			   20M:$DIR\testfile02:x `
			   20M:$DIR\testfile03:x `
			   R `
			   40M:$DIR\testfile14:x `
			   20M:$DIR\testfile15:x

# the pool not closed and part moved to another dimm
expect_abnormal_exit $Env:EXE_DIR\obj_sds 1 1 $POOLSET `
				   5 0 7 0 9 0 11 0 12 0

expect_abnormal_exit $Env:EXE_DIR\obj_sds 0 0 $POOLSET `
					 5 0 7 0 9 0 11 0 121 0

pass
