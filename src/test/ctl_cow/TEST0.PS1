# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019, Intel Corporation
#
# src/test/ctl_cow/TEST0 -- unit test which check copy on write feature
# for obj pools

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type any

setup

$POOL="$DIR\testfile"
$REPLICA="$DIR\testfile_replica"
$POOLSET="$DIR\pool0.set"

create_poolset $POOLSET `
    32M:$DIR\testfile:z `
	R `
    32M:$DIR\testfile_replica:z

expect_normal_exit $PMEMPOOL create obj $POOLSET

$POOL_BEFORE = $(Get-FileHash $POOL).hash
$REPLICA_BEFORE = $(Get-FileHash $REPLICA).hash

$Env:PMEMOBJ_CONF += "copy_on_write.at_open=1"

expect_normal_exit $Env:EXE_DIR\ctl_cow$Env:EXESUFFIX $POOLSET obj

$POOL_AFTER = $(Get-FileHash $POOL).hash
$REPLICA_AFTER = $(Get-FileHash $REPLICA).hash

if ($POOLSET_BEFORE -ne $POOLSET_AFTER -Or $REPLICA_BEFORE -ne $REPLICA_AFTER) {
	fatal '$POOLSET_BEFORE -ne $POOLSET_AFTER -or $REPLICA_BEFORE -ne $REPLICA_AFTER'
}
check

pass
