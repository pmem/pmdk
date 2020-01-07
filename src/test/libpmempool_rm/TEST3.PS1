# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2018, Intel Corporation
#
#
# libpmempool_rm/TEST3 -- test pmempool_rm with directories
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

enable_log_append

mkdir $DIR\dir.1 | out-null
mkdir $DIR\dir.2 | out-null
mkdir $DIR\dir.2\dir.3 | out-null

expect_normal_exit $Env:EXE_DIR\libpmempool_rm$Env:EXESUFFIX $DIR\dir.1 $DIR\dir.2 $DIR\dir.2\dir.3
expect_normal_exit $Env:EXE_DIR\libpmempool_rm$Env:EXESUFFIX -f $DIR\dir.1 $DIR\dir.2 $DIR\dir.2\dir.3

check

pass
