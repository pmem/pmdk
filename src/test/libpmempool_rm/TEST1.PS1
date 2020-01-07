# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2018, Intel Corporation
#
#
# libpmempool_rm/TEST0 -- test pmempool_rm with pool set files
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

enable_log_append

create_poolset $DIR\pool1p1r.set 32M:$DIR\pool1p1.obj:x
create_poolset $DIR\pool3p2r.set 32M:$DIR\pool2p1.obj:x `
	32M:$DIR\pool2p2.obj:x 32M:$DIR\pool2p3.obj:x `
	R 32M:$DIR\pool3p1.obj:x 32M:$DIR\pool3p2.obj:x 32M:$DIR\pool3p3.obj:x
expect_normal_exit $PMEMPOOL$EXESUFFIX create obj $DIR\pool1p1r.set
expect_normal_exit $PMEMPOOL$EXESUFFIX create obj $DIR\pool3p2r.set

check_files $DIR\pool1p1r.set $DIR\pool1p1.obj `
	$DIR\pool3p2r.set $DIR\pool2p1.obj `
	$DIR\pool2p2.obj $DIR\pool2p3.obj `
	$DIR\pool3p1.obj $DIR\pool3p2.obj $DIR\pool3p3.obj

# this should remove all the pool files
expect_normal_exit $Env:EXE_DIR\libpmempool_rm$Env:EXESUFFIX $DIR\pool1p1r.set $DIR\pool3p2r.set

# check if all pool files are removed
check_no_files $DIR\pool1p1.obj $DIR\pool2p1.obj `
	$DIR\pool2p2.obj $DIR\pool2p3.obj `
	$DIR\pool3p1.obj $DIR\pool3p2.obj $DIR\pool3p3.obj

# poolset files should exist
check_files $DIR\pool1p1r.set $DIR\pool3p2r.set

create_poolset $DIR\pool1p1r.set 32M:$DIR\pool1p1.obj:x
create_poolset $DIR\pool3p2r.set 32M:$DIR\pool2p1.obj:x `
	32M:$DIR\pool2p2.obj:x 32M:$DIR\pool2p3.obj:x `
	R 32M:$DIR\pool3p1.obj:x 32M:$DIR\pool3p2.obj:x 32M:$DIR\pool3p3.obj:x
expect_normal_exit $PMEMPOOL$EXESUFFIX create obj $DIR\pool1p1r.set
expect_normal_exit $PMEMPOOL$EXESUFFIX create obj $DIR\pool3p2r.set

check_files $DIR\pool1p1r.set $DIR\pool1p1.obj `
	$DIR\pool3p2r.set $DIR\pool2p1.obj `
	$DIR\pool2p2.obj $DIR\pool2p3.obj `
	$DIR\pool3p1.obj $DIR\pool3p2.obj $DIR\pool3p3.obj

# this should remove all the pool files and pool sets
expect_normal_exit $Env:EXE_DIR\libpmempool_rm$Env:EXESUFFIX -l $DIR\pool1p1r.set $DIR\pool3p2r.set

# check if all pool files are removed
check_no_files $DIR\pool1p1.obj $DIR\pool2p1.obj `
	$DIR\pool2p2.obj $DIR\pool2p3.obj `
	$DIR\pool3p1.obj $DIR\pool3p2.obj $DIR\pool3p3.obj `
	$DIR\pool1p1r.set $DIR\pool3p2r.set

# this should report errors for all files
expect_normal_exit $Env:EXE_DIR\libpmempool_rm$Env:EXESUFFIX $DIR\pool1p1r.set $DIR\pool3p2r.set

# this should ignore all errors
expect_normal_exit $Env:EXE_DIR\libpmempool_rm$Env:EXESUFFIX -f $DIR\pool1p1r.set $DIR\pool3p2r.set

check

pass
