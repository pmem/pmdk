# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2018, Intel Corporation
#
#
# libpmempool_backup/TEST3 -- test backup poolset with replica (invalid)
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type pmem non-pmem

setup

. .\common.PS1

for ($i=0; $i -lt $POOL_TYPES.Count; $i++ ) {
	backup_cleanup

	# prepare poolset files
	create_poolset_variation 1
	create_poolset_variation 2 $BACKUP

	# create source poolset parts
	expect_normal_exit $PMEMPOOL create $POOL_TYPES[$i] `
		$POOL_CREATE_PARAMS[$i] $POOLSET

	backup_and_compare $POOLSET $POOL_TYPES[$i]
}

rm $OUT -Force -ea si
mv $OUT_TEMP $OUT

check

pass
