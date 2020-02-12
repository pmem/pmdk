# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2018, Intel Corporation
#
#
# libpmempool_backup/TEST7 -- test backup to pool which already exists
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type pmem non-pmem

setup

. .\common.PS1

for ($i=0; $i -lt $POOL_TYPES.Count; $i++ ) {
	backup_cleanup

	$POOLFILE= -join("$DIR\pool.", $POOL_TYPES[$i]);
	rm $POOLFILE -Force -ea si

	# create source poolset
	expect_normal_exit $PMEMPOOL create $POOL_TYPES[$i] `
		$POOL_CREATE_PARAMS[$i] --size 20M $POOLFILE

	# create backup pool file
	truncate -s 20M $POOLFILE$BACKUP
	backup_and_compare $POOLFILE $POOL_TYPES[$i]

	# create too small backup pool file
	truncate -s 10M $POOLFILE$BACKUP
	backup_and_compare $POOLFILE $POOL_TYPES[$i]
}
rm $OUT -Force -ea si
mv $OUT_TEMP $OUT

check

pass
