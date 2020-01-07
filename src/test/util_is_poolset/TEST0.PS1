# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# src/test/util_is_poolset/TEST0 -- unit test for util_is_poolset
#
#
# parameter handling
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

create_poolset $DIR\pool.set 32K:$DIR\testfile:x
expect_normal_exit $PMEMPOOL create obj $DIR\pool.obj

Set-Content -literalpath $DIR\not_pool.set -Value "NON_PMEMPOOLSET"
Set-Content -literalpath $DIR\too_short.set -Value "PMEMPOOL"

touch $DIR\empty.set
expect_normal_exit $Env:EXE_DIR\util_is_poolset$Env:EXESUFFIX `
    $DIR\pool.set `
    $DIR\not_pool.set `
    $DIR\too_short.set `
    $DIR\pool.obj `
    $DIR\empty.set `
    $DIR\not_exist.set

check

pass
